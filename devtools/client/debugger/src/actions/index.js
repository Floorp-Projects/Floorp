/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as ast from "./ast/index";
import * as breakpoints from "./breakpoints/index";
import * as exceptions from "./exceptions";
import * as expressions from "./expressions";
import * as eventListeners from "./event-listeners";
import * as pause from "./pause/index";
import * as navigation from "./navigation";
import * as ui from "./ui";
import * as fileSearch from "./file-search";
import * as projectTextSearch from "./project-text-search";
import * as quickOpen from "./quick-open";
import * as sourcesTree from "./sources-tree";
import * as sources from "./sources/index";
import * as sourcesActors from "./source-actors";
import * as tabs from "./tabs";
import * as threads from "./threads";
import * as toolbox from "./toolbox";
import * as preview from "./preview";
import * as tracing from "./tracing";
import * as contextMenu from "./context-menus/index";

import { objectInspector } from "devtools/client/shared/components/reps/index";

export default {
  ...ast,
  ...navigation,
  ...breakpoints,
  ...exceptions,
  ...expressions,
  ...eventListeners,
  ...sources,
  ...sourcesActors,
  ...tabs,
  ...pause,
  ...ui,
  ...fileSearch,
  ...objectInspector.actions,
  ...projectTextSearch,
  ...quickOpen,
  ...sourcesTree,
  ...threads,
  ...toolbox,
  ...preview,
  ...tracing,
  ...contextMenu,
};
