/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useCallback, useMemo, useRef } from "react";
import { Localized, CONFIGURABLE_STYLES } from "./MSLocalized";

const MULTI_SELECT_STYLES = [
  ...CONFIGURABLE_STYLES,
  "flexDirection",
  "flexWrap",
  "flexFlow",
  "flexGrow",
  "flexShrink",
  "justifyContent",
  "alignItems",
  "gap",
];

const MULTI_SELECT_ICON_STYLES = [
  ...CONFIGURABLE_STYLES,
  "width",
  "height",
  "background",
  "backgroundColor",
  "backgroundImage",
  "backgroundSize",
  "backgroundPosition",
  "backgroundRepeat",
  "backgroundOrigin",
  "backgroundClip",
  "border",
  "borderRadius",
  "appearance",
  "fill",
  "stroke",
  "outline",
  "outlineOffset",
  "boxShadow",
];

function getValidStyle(style, validStyles, allowVars) {
  if (!style) {
    return null;
  }
  return Object.keys(style)
    .filter(
      key => validStyles.includes(key) || (allowVars && key.startsWith("--"))
    )
    .reduce((obj, key) => {
      obj[key] = style[key];
      return obj;
    }, {});
}

export const MultiSelect = ({
  content,
  activeMultiSelect,
  setActiveMultiSelect,
}) => {
  const { data } = content.tiles;

  const refs = useRef({});

  const handleChange = useCallback(() => {
    const newActiveMultiSelect = [];
    Object.keys(refs.current).forEach(key => {
      if (refs.current[key]?.checked) {
        newActiveMultiSelect.push(key);
      }
    });
    setActiveMultiSelect(newActiveMultiSelect);
  }, [setActiveMultiSelect]);

  const containerStyle = useMemo(
    () => getValidStyle(content.tiles.style, MULTI_SELECT_STYLES, true),
    [content.tiles.style]
  );

  // When screen renders for first time, update state
  // with checkbox ids that has defaultvalue true
  useEffect(() => {
    if (!activeMultiSelect) {
      let newActiveMultiSelect = [];
      data.forEach(({ id, defaultValue }) => {
        if (defaultValue && id) {
          newActiveMultiSelect.push(id);
        }
      });
      setActiveMultiSelect(newActiveMultiSelect);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <div className="multi-select-container" style={containerStyle}>
      {data.map(({ id, label, icon, type = "checkbox", group, style }) => (
        <div
          key={id + label}
          className="checkbox-container multi-select-item"
          style={getValidStyle(style, MULTI_SELECT_STYLES)}
        >
          <input
            type={type} // checkbox or radio
            id={id}
            value={id}
            name={group}
            checked={activeMultiSelect?.includes(id)}
            style={getValidStyle(icon?.style, MULTI_SELECT_ICON_STYLES)}
            onChange={handleChange}
            ref={el => (refs.current[id] = el)}
          />
          {label ? (
            <Localized text={label}>
              <label htmlFor={id}></label>
            </Localized>
          ) : null}
        </div>
      ))}
    </div>
  );
};
