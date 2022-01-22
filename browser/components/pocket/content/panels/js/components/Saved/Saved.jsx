/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import Header from "../Header/Header";

function Saved(props) {
  return (
    <>
      <Header>
        <a>
          <span data-l10n-id="pocket-panel-header-my-list"></span>
        </a>
      </Header>
    </>
  );
}

export default Saved;
