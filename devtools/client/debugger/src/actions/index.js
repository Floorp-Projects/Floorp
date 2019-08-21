/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import * as ast from "./ast";
import * as breakpoints from "./breakpoints";
import * as expressions from "./expressions";
import * as eventListeners from "./event-listeners";
import * as pause from "./pause";
import * as navigation from "./navigation";
import * as ui from "./ui";
import * as fileSearch from "./file-search";
import * as projectTextSearch from "./project-text-search";
import * as quickOpen from "./quick-open";
import * as sourceTree from "./source-tree";
import * as sources from "./sources";
import * as sourcesActors from "./source-actors";
import * as tabs from "./tabs";
import * as threads from "./threads";
import * as toolbox from "./toolbox";
import * as preview from "./preview";

export default {
  ...ast,
  ...navigation,
  ...breakpoints,
  ...expressions,
  ...eventListeners,
  ...sources,
  ...sourcesActors,
  ...tabs,
  ...pause,
  ...ui,
  ...fileSearch,
  ...projectTextSearch,
  ...quickOpen,
  ...sourceTree,
  ...threads,
  ...toolbox,
  ...preview,
};
