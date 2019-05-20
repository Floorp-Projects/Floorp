/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";

type ExceptionOptionProps = {
  className: string,
  isChecked: boolean,
  label: string,
  onChange: Function,
};

export default function ExceptionOption({
  className,
  isChecked = false,
  label,
  onChange,
}: ExceptionOptionProps) {
  return (
    <div className={className} onClick={onChange}>
      <input
        type="checkbox"
        checked={isChecked ? "checked" : ""}
        onChange={e => e.stopPropagation() && onChange()}
      />
      <div className="breakpoint-exceptions-label">{label}</div>
    </div>
  );
}
