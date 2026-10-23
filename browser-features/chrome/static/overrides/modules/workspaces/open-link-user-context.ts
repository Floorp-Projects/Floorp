/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  OpenLinkUserContextInput,
  OpenLinkUserContextOptions,
  WorkspaceOpenLinkUserContextResolution,
} from "./open-link-user-context-types.ts";

export type {
  OpenLinkUserContextInput,
  WorkspaceOpenLinkUserContextResolution,
} from "./open-link-user-context-types.ts";

function optionalUserContextId(value: unknown): number | undefined {
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < 0) {
    throw new RangeError("userContextId must be a non-negative safe integer");
  }
  return value;
}

function browserUserContextId(candidate: unknown): number | undefined {
  if (typeof candidate !== "object" || candidate === null) return undefined;
  const source = candidate as {
    browsingContext?: { originAttributes?: { userContextId?: unknown } };
    linkedBrowser?: {
      browsingContext?: { originAttributes?: { userContextId?: unknown } };
    };
  };
  const actual = optionalUserContextId(
    source.browsingContext?.originAttributes?.userContextId ??
      source.linkedBrowser?.browsingContext?.originAttributes?.userContextId,
  );
  if (actual !== undefined) return actual;
  const direct = optionalUserContextId(
    (candidate as { userContextId?: unknown }).userContextId,
  );
  if (direct !== undefined) return direct;
  const browser = candidate as { getAttribute?(name: string): string | null };
  if (typeof browser.getAttribute !== "function") return undefined;
  return optionalUserContextId(
    Number(browser.getAttribute("usercontextid") || 0),
  );
}

function originatingUserContextId(
  input: OpenLinkUserContextInput,
): number | undefined {
  const info = input.openWindowInfo as
    | {
      originAttributes?: { userContextId?: unknown };
    }
    | null
    | undefined;
  return optionalUserContextId(info?.originAttributes?.userContextId) ??
    browserUserContextId(input.openerBrowser) ??
    browserUserContextId(input.openerTab) ??
    browserUserContextId(input.targetBrowser);
}

export function resolveWorkspaceOpenLinkUserContext(
  input: OpenLinkUserContextInput | undefined,
  where: string,
  workspaceUserContextId: number | (() => number),
): WorkspaceOpenLinkUserContextResolution {
  const baseInput = input ?? {};
  const originalUserContextId = optionalUserContextId(baseInput.userContextId);

  if (where === "current") {
    const options: OpenLinkUserContextOptions = {
      ...baseInput,
      userContextId: originalUserContextId,
    };

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

  const originUserContextId = originalUserContextId === undefined
    ? originatingUserContextId(baseInput)
    : undefined;

  const userContextId = originalUserContextId ?? originUserContextId ??
    optionalUserContextId(
      typeof workspaceUserContextId === "function"
        ? workspaceUserContextId()
        : workspaceUserContextId,
    ) ?? 0;
  const shouldApplyWorkspaceContainer = userContextId > 0 &&
    originalUserContextId === undefined &&
    originUserContextId === undefined;

  return {
    options: {
      ...baseInput,
      userContextId,
    },
    originalUserContextId,
    shouldApplyWorkspaceContainer,
  };
}
