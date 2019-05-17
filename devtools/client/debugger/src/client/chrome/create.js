/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ChromeFrame, SourceLocation, LoadedObject } from "../../types";
import type { ServerLocation } from "./types";

export function fromServerLocation(
  serverLocation?: ServerLocation
): ?SourceLocation {
  if (serverLocation) {
    return {
      sourceId: serverLocation.scriptId,
      line: serverLocation.lineNumber + 1,
      column: serverLocation.columnNumber,
      sourceUrl: "",
    };
  }
}

export function toServerLocation(location: SourceLocation): ServerLocation {
  return {
    scriptId: location.sourceId,
    lineNumber: location.line - 1,
  };
}

export function createFrame(frame: any): ?ChromeFrame {
  const location = fromServerLocation(frame.location);
  if (!location) {
    return null;
  }

  return {
    id: frame.callFrameId,
    displayName: frame.functionName,
    scopeChain: frame.scopeChain,
    generatedLocation: location,
    location,
  };
}

export function createLoadedObject(
  serverObject: any,
  parentId: string
): LoadedObject {
  const { value, name } = serverObject;

  return {
    objectId: value.objectId,
    parentId,
    name,
    value,
  };
}
