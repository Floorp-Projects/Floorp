export const selectLayoutRender = (state, prefs, rickRollCache) => {
  const {layout, feeds, spocs} = state;
  let spocIndex = 0;
  let bufferRollCache = [];
  // Records the chosen and unchosen spocs by the probability selection.
  let chosenSpocs = new Set();
  let unchosenSpocs = new Set();

  // rickRollCache stores random probability values for each spoc position. This cache is empty
  // on page refresh and gets filled with random values on first render inside maybeInjectSpocs.
  const isFirstRun = !rickRollCache.length;

  function maybeInjectSpocs(data, spocsConfig) {
    if (data &&
        spocsConfig && spocsConfig.positions && spocsConfig.positions.length &&
        spocs.data.spocs && spocs.data.spocs.length) {
      const recommendations = [...data.recommendations];
      for (let position of spocsConfig.positions) {
        const spoc = spocs.data.spocs[spocIndex];
        if (!spoc) {
          break;
        }

        // Cache random number for a position
        let rickRoll;
        if (isFirstRun) {
          rickRoll = Math.random();
          rickRollCache.push(rickRoll);
        } else {
          rickRoll = rickRollCache.shift();
          bufferRollCache.push(rickRoll);
        }

        if (rickRoll <= spocsConfig.probability) {
          spocIndex++;
          recommendations.splice(position.index, 0, spoc);
          chosenSpocs.add(spoc);
        } else {
          unchosenSpocs.add(spoc);
        }
      }

      return {
        ...data,
        recommendations,
      };
    }

    return data;
  }

  const positions = {};
  const DS_COMPONENTS = ["Message", "SectionTitle", "Navigation",
    "CardGrid", "Hero", "HorizontalRule", "List"];

  const filterArray = [];

  if (!prefs["feeds.topsites"]) {
    filterArray.push("TopSites");
  }

  if (!prefs["feeds.section.topstories"]) {
    filterArray.push(...DS_COMPONENTS);
  }

  const layoutRender = layout.map(row => ({
    ...row,

    // Loops through desired components and adds a .data property
    // containing data from feeds
    components: row.components.filter(c => !filterArray.includes(c.type)).map(component => {
      if (!component.feed || !feeds.data[component.feed.url]) {
        return component;
      }

      positions[component.type] = positions[component.type] || 0;

      let {data} = feeds.data[component.feed.url];

      if (component && component.properties && component.properties.offset) {
        data = {
          ...data,
          recommendations: data.recommendations.slice(component.properties.offset),
        };
      }

      data = maybeInjectSpocs(data, component.spocs);

      // If empty, fill rickRollCache with random probability values from bufferRollCache
      if (!rickRollCache.length) {
        rickRollCache.push(...bufferRollCache);
      }

      let items = 0;
      if (component.properties && component.properties.items) {
        items = Math.min(component.properties.items, data.recommendations.length);
      }

      // loop through a component items
      // Store the items position sequentially for multiple components of the same type.
      // Example: A second card grid starts pos offset from the last card grid.
      for (let i = 0; i < items; i++) {
        data.recommendations[i].pos = positions[component.type]++;
      }

      return {...component, data};
    }),
  })).filter(row => row.components.length);

  // Generate the payload for the SPOCS Fill ping. Note that a SPOC could be rejected
  // by the `probability_selection` first, then gets chosen for the next position. For
  // all other SPOCS that never went through the probabilistic selection, its reason will
  // be "out_of_position".
  let spocsFill = [];
  if (spocs.data.spocs) {
    const chosenSpocsFill = [...chosenSpocs]
      .map(spoc => ({id: spoc.id, reason: "n/a", displayed: 1, full_recalc: 0}));
    const unchosenSpocsFill = [...unchosenSpocs]
      .filter(spoc => !chosenSpocs.has(spoc))
      .map(spoc => ({id: spoc.id, reason: "probability_selection", displayed: 0, full_recalc: 0}));
    const outOfPositionSpocsFill = spocs.data.spocs.slice(spocIndex)
      .filter(spoc => !unchosenSpocs.has(spoc))
      .map(spoc => ({id: spoc.id, reason: "out_of_position", displayed: 0, full_recalc: 0}));

    spocsFill = [
      ...chosenSpocsFill,
      ...unchosenSpocsFill,
      ...outOfPositionSpocsFill,
    ];
  }

  return {spocsFill, layoutRender};
};
