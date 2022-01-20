/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import { Localized } from "./MSLocalized";

// Click of variation should call handle action
// passing 'colorway-variationId' theme name as event target value
// For default colorway , theme value passed to handle action
// includes just variation.id e.g. Light, Dark, Automatic
export const VariationsCircle = props => {
  const {
    activeTheme,
    colorway,
    colorwayText,
    nextColor,
    setVariation,
    transition,
    variations,
  } = props;
  return (
    <div
      className={`colorway-variations ${colorway} ${transition}`}
      next={nextColor}
    >
      <div className="variations-disc" />
      <Localized text={colorwayText}>
        <div className="colorway-text" />
      </Localized>
      {variations?.map(({ id, label, tooltip, description }) => (
        <Localized key={id} text={typeof tooltip === "object" ? tooltip : {}}>
          <label className="theme colorway" title={label}>
            <Localized
              text={typeof description === "object" ? description : {}}
            >
              <input
                type="radio"
                value={colorway === "default" ? id : `${colorway}-${id}`}
                checked={activeTheme?.includes(id)}
                name="variationSelect"
                className="sr-only input"
                onClick={setVariation}
                data-l10n-attrs="aria-description"
              />
            </Localized>
            <Localized text={label}>
              <div
                className={`text variation-button ${
                  activeTheme?.includes(id) ? " selected" : ""
                }`}
              />
            </Localized>
          </label>
        </Localized>
      ))}
    </div>
  );
};

// Return colorway as "default" for default theme variations Automatic, Light, Dark
// and Alpenglow theme which is not supported in Colorway picker
// For themes other then default, theme names exist in
// format colorway-variationId inside LIGHT_WEIGHT_THEMES in AboutWelcomeParent
export function computeColorWay(themeName, systemVariations) {
  return !themeName ||
    themeName === "alpenglow" ||
    systemVariations.find(variation => themeName === variation.id)
    ? "default"
    : themeName.split("-")[0];
}

export function Colorways(props) {
  let {
    colorways,
    defaultVariationIndex,
    systemVariations,
    variations,
  } = props.content.tiles;

  // This sets a default value
  const activeId = computeColorWay(props.activeTheme, systemVariations);
  const [colorwayId, setState] = useState(activeId);

  // Update state any time activeTheme changes.
  useEffect(() => {
    setState(computeColorWay(props.activeTheme, systemVariations));
  }, [props.activeTheme]);

  // Allow "in" style to render to actually transition towards regular state.
  const [transition, setTransition] = useState("");
  useEffect(() => {
    if (transition === "in") {
      // Figure out the variation to activate based on the active theme. Check
      // if it's a system variant then colorway variant falling back to default.
      let variationIndex = systemVariations.findIndex(
        ({ id }) => id === props.activeTheme
      );
      if (variationIndex < 0) {
        variationIndex = variations.findIndex(({ id }) =>
          props.activeTheme.includes(id)
        );
      }
      if (variationIndex < 0) {
        // This content config default assumes it's been selected correctly to
        // index into both `systemVariations` or `variations` (also configured).
        variationIndex = defaultVariationIndex;
      }

      // Simulate a color click event now that we're ready to transition in.
      props.handleAction({
        currentTarget: {
          value:
            colorwayId === "default"
              ? systemVariations[variationIndex].id
              : `${colorwayId}-${variations[variationIndex].id}`,
        },
      });

      // Trigger the transition from "in" to normal.
      requestAnimationFrame(() =>
        requestAnimationFrame(() => setTransition(""))
      );
    }
  }, [transition]);

  // Called on click of Colorway circle that sets the next colorway state and
  // starts transitions if not already started.
  function handleColorwayClick(event) {
    setState(event.currentTarget.dataset.colorway);
    if (transition !== "out") {
      setTransition("out");
      setTimeout(() => setTransition("in"), 500);
    }
  }

  return (
    <div className="tiles-theme-container colorway">
      <div>
        <fieldset className="tiles-theme-section">
          <Localized text={props.content.subtitle}>
            <legend className="sr-only" />
          </Localized>
          {colorways.map(({ id, label, tooltip, description }) => (
            <Localized
              key={id + label}
              text={typeof tooltip === "object" ? tooltip : {}}
            >
              <label
                className="theme colorway"
                title={label}
                data-l10n-args={JSON.stringify({
                  colorwayName: label,
                })}
              >
                <Localized
                  text={typeof description === "object" ? description : {}}
                >
                  <span
                    className="sr-only colorway label"
                    id={`${id}-label`}
                    data-l10n-args={JSON.stringify({
                      colorwayName: label,
                    })}
                  />
                </Localized>
                <Localized
                  text={typeof description === "object" ? description : {}}
                >
                  <input
                    type="radio"
                    data-colorway={id}
                    name="theme"
                    checked={colorwayId === id}
                    className="sr-only input"
                    onClick={handleColorwayClick}
                    data-l10n-attrs="aria-description"
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
      <VariationsCircle
        nextColor={colorwayId}
        transition={transition}
        variations={activeId === "default" ? systemVariations : variations}
        colorway={activeId}
        colorwayText={
          colorways.find(colorway => colorway.id === activeId)?.label
        }
        setVariation={props.handleAction}
        activeTheme={props.activeTheme}
      />
    </div>
  );
}
