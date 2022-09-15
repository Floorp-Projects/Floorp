/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import { Localized } from "./MSLocalized";

export const ColorwayDescription = props => {
  const { colorway } = props;
  if (!colorway) {
    return null;
  }
  const { label, description } = colorway;
  return (
    <Localized text={description}>
      <div
        className="colorway-text"
        data-l10n-args={JSON.stringify({
          colorwayName: label,
        })}
      />
    </Localized>
  );
};

// Return colorway as "default" for default theme variations Automatic, Light, Dark,
// Alpenglow theme and legacy colorways which is not supported in Colorway picker.
// For themes other then default, theme names exist in
// format colorway-variationId inside LIGHT_WEIGHT_THEMES in AboutWelcomeParent
export function computeColorWay(themeName, systemVariations) {
  return !themeName ||
    themeName === "alpenglow" ||
    systemVariations.includes(themeName)
    ? "default"
    : themeName.split("-")[0];
}

// Set variationIndex based off activetheme value e.g. 'light', 'expressionist-soft'
export function computeVariationIndex(
  themeName,
  systemVariations,
  variations,
  defaultVariationIndex
) {
  // Check if themeName is in systemVariations, if yes choose variationIndex by themeName
  let index = systemVariations.findIndex(theme => theme === themeName);
  if (index >= 0) {
    return index;
  }

  // If themeName is one of the colorways, select variation index from colorways
  let variation = themeName?.split("-")[1];
  index = variations.findIndex(element => element === variation);
  if (index >= 0) {
    return index;
  }
  return defaultVariationIndex;
}

export function Colorways(props) {
  let {
    colorways,
    darkVariation,
    defaultVariationIndex,
    systemVariations,
    variations,
  } = props.content.tiles;
  let hasReverted = false;

  // Active theme id from JSON e.g. "expressionist"
  const activeId = computeColorWay(props.activeTheme, systemVariations);
  const [colorwayId, setState] = useState(activeId);
  const [variationIndex, setVariationIndex] = useState(defaultVariationIndex);

  function revertToDefaultTheme() {
    if (hasReverted) return;

    // Spoofing an event with current target value of "navigate_away"
    // helps the handleAction method to read the colorways theme as "revert"
    // which causes the initial theme to be activated.
    // The "navigate_away" action is set in content in the colorways screen JSON config.
    // Any value in the JSON for theme will work, provided it is not `<event>`.
    const event = {
      currentTarget: {
        value: "navigate_away",
      },
    };
    props.handleAction(event);
    hasReverted = true;
  }

  // Revert to default theme if the user navigates away from the page or spotlight modal
  // before clicking on the primary button to officially set theme.
  useEffect(() => {
    addEventListener("beforeunload", revertToDefaultTheme);
    addEventListener("pagehide", revertToDefaultTheme);

    return () => {
      removeEventListener("beforeunload", revertToDefaultTheme);
      removeEventListener("pagehide", revertToDefaultTheme);
    };
  });
  // Update state any time activeTheme changes.
  useEffect(() => {
    setState(computeColorWay(props.activeTheme, systemVariations));
    setVariationIndex(
      computeVariationIndex(
        props.activeTheme,
        systemVariations,
        variations,
        defaultVariationIndex
      )
    );
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [props.activeTheme]);

  //select a random colorway
  useEffect(() => {
    //We don't want the default theme to be selected
    const randomIndex = Math.floor(Math.random() * (colorways.length - 1)) + 1;
    const randomColorwayId = colorways[randomIndex].id;

    // Change the variation to be the dark variation if configured and dark.
    // Additional colorway changes will remain dark while system is unchanged.
    if (
      darkVariation !== undefined &&
      window.matchMedia("(prefers-color-scheme: dark)").matches
    ) {
      variations[variationIndex] = variations[darkVariation];
    }
    const value = `${randomColorwayId}-${variations[variationIndex]}`;
    props.handleAction({ currentTarget: { value } });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="tiles-theme-container">
      <div>
        <fieldset className="tiles-theme-section">
          <Localized text={props.content.subtitle}>
            <legend className="sr-only" />
          </Localized>
          {colorways.map(({ id, label, tooltip }) => (
            <Localized
              key={id + label}
              text={typeof tooltip === "object" ? tooltip : {}}
            >
              <label
                className="theme"
                title={label}
                data-l10n-args={JSON.stringify({
                  colorwayName: label,
                })}
              >
                <Localized text={typeof label === "object" ? label : {}}>
                  <span
                    className="sr-only colorway label"
                    id={`${id}-label`}
                    data-l10n-args={JSON.stringify({
                      colorwayName: label,
                    })}
                  />
                </Localized>
                <Localized text={typeof label === "object" ? label : {}}>
                  <input
                    type="radio"
                    data-colorway={id}
                    name="theme"
                    value={
                      id === "default"
                        ? systemVariations[variationIndex]
                        : `${id}-${variations[variationIndex]}`
                    }
                    checked={colorwayId === id}
                    className="sr-only input"
                    onClick={props.handleAction}
                    data-l10n-args={JSON.stringify({
                      colorwayName: label,
                    })}
                    aria-labelledby={`${id}-label`}
                  />
                </Localized>
                <div
                  className={`icon colorway ${
                    colorwayId === id ? "selected" : ""
                  } ${id}`}
                />
              </label>
            </Localized>
          ))}
        </fieldset>
      </div>
      <ColorwayDescription
        colorway={colorways.find(colorway => colorway.id === activeId)}
      />
    </div>
  );
}
