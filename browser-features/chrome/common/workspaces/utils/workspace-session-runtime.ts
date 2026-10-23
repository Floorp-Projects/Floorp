// SPDX-License-Identifier: MPL-2.0

export const getWorkspaceSessionRuntime = () =>
  (ChromeUtils.importESModule(
    "resource://noraneko/modules/WorkspaceSessions.sys.mjs",
  ) as typeof import("#modules/modules/WorkspaceSessions.sys.mts"))
    .WorkspaceSessions;
