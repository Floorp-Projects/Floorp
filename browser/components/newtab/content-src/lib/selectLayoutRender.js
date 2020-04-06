/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const selectLayoutRender = ({
  state = {},
  prefs = {},
  rollCache = [],
  locale = "",
}) => {
  const { layout, feeds, spocs } = state;
  let spocIndexMap = {};
  let bufferRollCache = [];
  // Records the chosen and unchosen spocs by the probability selection.
  let chosenSpocs = new Set();
  let unchosenSpocs = new Set();

  function rollForSpocs(data, spocsConfig, spocsData, placementName) {
    if (!spocIndexMap[placementName] && spocIndexMap[placementName] !== 0) {
      spocIndexMap[placementName] = 0;
    }
    const results = [...data];
    for (let position of spocsConfig.positions) {
      const spoc = spocsData[spocIndexMap[placementName]];
      if (!spoc) {
        break;
      }

      // Cache random number for a position
      let rickRoll;
      if (!rollCache.length) {
        rickRoll = Math.random();
        bufferRollCache.push(rickRoll);
      } else {
        rickRoll = rollCache.shift();
        bufferRollCache.push(rickRoll);
      }

      if (rickRoll <= spocsConfig.probability) {
        spocIndexMap[placementName]++;
        if (!spocs.blocked.includes(spoc.url)) {
          results.splice(position.index, 0, spoc);
          chosenSpocs.add(spoc);
        }
      } else {
        unchosenSpocs.add(spoc);
      }
    }

    return results;
  }

  const positions = {};
  const DS_COMPONENTS = [
    "Message",
    "TextPromo",
    "SectionTitle",
    "Navigation",
    "CardGrid",
    "CollectionCardGrid",
    "Hero",
    "HorizontalRule",
    "List",
  ];

  const filterArray = [];

  if (!prefs["feeds.topsites"]) {
    filterArray.push("TopSites");
  }

  if (!locale.startsWith("en-")) {
    filterArray.push("Navigation");
  }

  if (!prefs["feeds.section.topstories"]) {
    filterArray.push(...DS_COMPONENTS);
  }

  const placeholderComponent = component => {
    if (!component.feed) {
      // TODO we now need a placeholder for topsites and textPromo.
      return {
        ...component,
        data: {
          spocs: [],
        },
      };
    }
    const data = {
      recommendations: [],
    };

    let items = 0;
    if (component.properties && component.properties.items) {
      items = component.properties.items;
    }
    for (let i = 0; i < items; i++) {
      data.recommendations.push({ placeholder: true });
    }

    return { ...component, data };
  };

  // TODO update devtools to show placements
  const handleSpocs = (data, component) => {
    let result = [...data];
    // Do we ever expect to possibly have a spoc.
    if (
      component.spocs &&
      component.spocs.positions &&
      component.spocs.positions.length
    ) {
      const placement = component.placement || {};
      const placementName = placement.name || "spocs";
      const spocsData = spocs.data[placementName];
      // We expect a spoc, spocs are loaded, and the server returned spocs.
      if (
        spocs.loaded &&
        spocsData &&
        spocsData.items &&
        spocsData.items.length
      ) {
        result = rollForSpocs(
          result,
          component.spocs,
          spocsData.items,
          placementName
        );
      }
    }
    return result;
  };

  const handleComponent = component => {
    if (
      component.spocs &&
      component.spocs.positions &&
      component.spocs.positions.length
    ) {
      const placement = component.placement || {};
      const placementName = placement.name || "spocs";
      const spocsData = spocs.data[placementName];
      if (
        spocs.loaded &&
        spocsData &&
        spocsData.items &&
        spocsData.items.length
      ) {
        return {
          ...component,
          data: {
            spocs: spocsData.items
              .filter(spoc => spoc && !spocs.blocked.includes(spoc.url))
              .map((spoc, index) => ({
                ...spoc,
                pos: index,
              })),
          },
        };
      }
    }
    return {
      ...component,
      data: {
        spocs: [],
      },
    };
  };

  const handleComponentWithFeed = component => {
    positions[component.type] = positions[component.type] || 0;
    let data = {
      recommendations: [],
    };

    const feed = feeds.data[component.feed.url];
    if (feed && feed.data) {
      data = {
        ...feed.data,
        recommendations: [...(feed.data.recommendations || [])],
      };
    }

    if (component && component.properties && component.properties.offset) {
      data = {
        ...data,
        recommendations: data.recommendations.slice(
          component.properties.offset
        ),
      };
    }

    data = {
      ...data,
      recommendations: handleSpocs(data.recommendations, component),
    };

    let items = 0;
    if (component.properties && component.properties.items) {
      items = Math.min(component.properties.items, data.recommendations.length);
    }

    // loop through a component items
    // Store the items position sequentially for multiple components of the same type.
    // Example: A second card grid starts pos offset from the last card grid.
    for (let i = 0; i < items; i++) {
      data.recommendations[i] = {
        ...data.recommendations[i],
        pos: positions[component.type]++,
      };
    }

    return { ...component, data };
  };

  const renderLayout = () => {
    const renderedLayoutArray = [];
    for (const row of layout.filter(
      r => r.components.filter(c => !filterArray.includes(c.type)).length
    )) {
      let components = [];
      renderedLayoutArray.push({
        ...row,
        components,
      });
      for (const component of row.components.filter(
        c => !filterArray.includes(c.type)
      )) {
        const spocsConfig = component.spocs;
        if (spocsConfig || component.feed) {
          // TODO make sure this still works for different loading cases.
          if (
            (component.feed && !feeds.data[component.feed.url]) ||
            (spocsConfig &&
              spocsConfig.positions &&
              spocsConfig.positions.length &&
              !spocs.loaded)
          ) {
            components.push(placeholderComponent(component));
            return renderedLayoutArray;
          }
          if (component.feed) {
            components.push(handleComponentWithFeed(component));
          } else {
            components.push(handleComponent(component));
          }
        } else {
          components.push(component);
        }
      }
    }
    return renderedLayoutArray;
  };

  const layoutRender = renderLayout();

  // If empty, fill rollCache with random probability values from bufferRollCache
  if (!rollCache.length) {
    rollCache.push(...bufferRollCache);
  }

  // Generate the payload for the SPOCS Fill ping. Note that a SPOC could be rejected
  // by the `probability_selection` first, then gets chosen for the next position. For
  // all other SPOCS that never went through the probabilistic selection, its reason will
  // be "out_of_position".
  let spocsFill = [];
  if (
    spocs.loaded &&
    feeds.loaded &&
    spocs.data.spocs &&
    spocs.data.spocs.items
  ) {
    const chosenSpocsFill = [...chosenSpocs].map(spoc => ({
      id: spoc.id,
      reason: "n/a",
      displayed: 1,
      full_recalc: 0,
    }));
    const unchosenSpocsFill = [...unchosenSpocs]
      .filter(spoc => !chosenSpocs.has(spoc))
      .map(spoc => ({
        id: spoc.id,
        reason: "probability_selection",
        displayed: 0,
        full_recalc: 0,
      }));
    const outOfPositionSpocsFill = spocs.data.spocs.items
      .slice(spocIndexMap.spocs)
      .filter(spoc => !unchosenSpocs.has(spoc))
      .map(spoc => ({
        id: spoc.id,
        reason: "out_of_position",
        displayed: 0,
        full_recalc: 0,
      }));

    spocsFill = [
      ...chosenSpocsFill,
      ...unchosenSpocsFill,
      ...outOfPositionSpocsFill,
    ];
  }

  return { spocsFill, layoutRender };
};
