/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

import { connect as reduxConnect } from "react-redux";
import * as React from "react";

export function connect<Config, RSP: {}, MDP: {}>(
  mapStateToProps: (state: any, props: any) => RSP,
  mapDispatchToProps?: (Function => MDP) | MDP,
  mergeProps?: void,
  opts?: ?{|
    storeKey?: string,
  |}
): (
  Component: React.AbstractComponent<Config>
) => React.AbstractComponent<$Diff<Config, RSP & MDP>> {
  // TODO: Bug 1572214 - We should use the standard type definitions directly.
  // $FlowFixMe
  return reduxConnect(mapStateToProps, mapDispatchToProps, null, opts);
}
