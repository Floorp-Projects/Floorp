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
  let { variations, colorway, colorwayText, activeTheme, setVariation } = props;
  return (
    <div className={`colorway-variations ${colorway}`}>
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
// For themes other then default, theme names exist in
// format colorway-variationId inside LIGHT_WEIGHT_THEMES in AboutWelcomeParent
export function computeColorWay(themeName, systemVariations) {
  return !themeName ||
    systemVariations.find(variation => themeName === variation.id)
    ? "default"
    : themeName.split("-")[0];
}

export function Colorways(props) {
  let {
    colorways,
    defaultVariationId,
    systemDefaultVariationId,
    systemVariations,
    variations,
  } = props.content.tiles;

  // This sets a default value
  const [colorwayId, setState] = useState(
    computeColorWay(props.activeTheme, systemVariations)
  );

  // Update state any time activeTheme changes.
  useEffect(() => {
    setState(computeColorWay(props.activeTheme, systemVariations));
  }, [props.activeTheme]);

  // Called on click of Colorway circle that sets colorway state
  // used to pass selected colorway to variation circle and
  // call handleAction passing 'colorway-defaultvariationId' as event target value
  function handleColorwayClick(event) {
    setState(event.currentTarget.dataset.colorway);
    props.handleAction(event);
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
                  <input
                    type="radio"
                    data-colorway={id}
                    value={
                      id === "default"
                        ? systemDefaultVariationId
                        : `${id}-${defaultVariationId}`
                    }
                    name="theme"
                    checked={computeColorWay(
                      props.activeTheme,
                      systemVariations
                    )?.includes(id)}
                    className="sr-only input"
                    onClick={handleColorwayClick}
                    data-l10n-attrs="aria-description"
                    data-l10n-args={JSON.stringify({
                      colorwayName: label,
                    })}
                  />
                </Localized>
                <div
                  className={`icon colorway ${
                    computeColorWay(
                      props.activeTheme,
                      systemVariations
                    )?.includes(id)
                      ? " selected"
                      : ""
                  } ${id}`}
                />
              </label>
            </Localized>
          ))}
        </fieldset>
      </div>
      <VariationsCircle
        variations={colorwayId === "default" ? systemVariations : variations}
        colorway={colorwayId}
        colorwayText={
          colorways.find(colorway => colorway.id === colorwayId)?.label
        }
        setVariation={props.handleAction}
        activeTheme={props.activeTheme}
      />
    </div>
  );
}
