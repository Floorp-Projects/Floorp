/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function Header(props) {
  return (
    <h1 className="stp_header">
      <div className="stp_header_logo" />
      {props.children}
    </h1>
  );
}

export default Header;
