/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";
import { Localized } from "./MSLocalized";

export const MultiSelect = props => {
  let handleChange = event => {
    if (event.currentTarget.checked) {
      props.setActiveMultiSelect([
        ...props.activeMultiSelect,
        event.currentTarget.value,
      ]);
    } else {
      props.setActiveMultiSelect(
        props.activeMultiSelect.filter(id => id !== event.currentTarget.value)
      );
    }
  };

  let { data } = props.content.tiles;
  // When screen renders for first time, update state
  // with checkbox ids that has defaultvalue true
  useEffect(() => {
    if (!props.activeMultiSelect) {
      props.setActiveMultiSelect(
        data.map(item => item.defaultValue && item.id).filter(item => !!item)
      );
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="multi-select-container">
      {props.content.tiles.data.map(({ label, id }) => (
        <div key={id + label} className="checkbox-container multi-select-item">
          <input
            type="checkbox"
            id={id}
            value={id}
            checked={props.activeMultiSelect?.includes(id)}
            onChange={handleChange}
          />
          <Localized text={label}>
            <label htmlFor={id}></label>
          </Localized>
        </div>
      ))}
    </div>
  );
};
