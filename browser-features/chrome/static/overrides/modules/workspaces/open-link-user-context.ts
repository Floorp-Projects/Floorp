/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type OpenLinkUserContextInput = {
  userContextId?: number;
  targetBrowser?: unknown;
  [key: string]: unknown;
};

export type WorkspaceOpenLinkUserContextResolution = {
  options: OpenLinkUserContextInput;
  originalUserContextId: number | undefined;
  shouldApplyWorkspaceContainer: boolean;
};

export function resolveWorkspaceOpenLinkUserContext(
  input: OpenLinkUserContextInput | undefined,
  where: string,
  workspaceUserContextId: number,
): WorkspaceOpenLinkUserContextResolution {
  const baseInput = input ?? {};
  const originalUserContextId = typeof baseInput.userContextId === "number"
    ? baseInput.userContextId
    : undefined;

  if (where === "current") {
    const options = { ...baseInput };

    if (originalUserContextId === undefined) {
      delete options.userContextId;
    } else {
      options.userContextId = originalUserContextId;
    }

    return {
      options,
      originalUserContextId,
      shouldApplyWorkspaceContainer: false,
    };
  }

  const potentialTargetBrowser = baseInput.targetBrowser;
  const targetBrowserUserContextId =
    typeof potentialTargetBrowser === "object" &&
      potentialTargetBrowser !== null &&
      typeof (potentialTargetBrowser as { userContextId?: unknown })
          .userContextId === "number"
      ? (potentialTargetBrowser as { userContextId: number }).userContextId
      : undefined;

  const hasUserContextId = "userContextId" in baseInput;
  const shouldApplyWorkspaceContainer = workspaceUserContextId > 0 &&
    !hasUserContextId &&
    targetBrowserUserContextId === undefined;

  return {
    options: {
      ...baseInput,
      userContextId: shouldApplyWorkspaceContainer
        ? workspaceUserContextId
        : (originalUserContextId ?? targetBrowserUserContextId ?? 0),
    },
    originalUserContextId,
    shouldApplyWorkspaceContainer,
  };
}
