/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import classnames from "classnames";

import "./AccessibleImage.css";

const AccessibleImage = (props: Object) => {
  props = {
    ...props,
    className: classnames("img", props.className),
  };
  return <span {...props} />;
};

export default AccessibleImage;
