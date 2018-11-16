/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React from "react";

import { getPauseReason } from "../../../utils/pause";
import type { Grip, ExceptionReason } from "../../../types";

import "./WhyPaused.css";

function renderExceptionSummary(exception: string | Grip) {
  if (typeof exception === "string") {
    return exception;
  }

  const preview = exception.preview;
  if (!preview || !preview.name || !preview.message) {
    return;
  }

  return `${preview.name}: ${preview.message}`;
}

function renderMessage(why: ExceptionReason) {
  if (why.type == "exception" && why.exception) {
    return (
      <div className={"message warning"}>
        {renderExceptionSummary(why.exception)}
      </div>
    );
  }

  if (typeof why.message == "string") {
    return <div className={"message"}>{why.message}</div>;
  }

  return null;
}

export default function renderWhyPaused(why: Object) {
  const reason = getPauseReason(why);

  if (!reason) {
    return null;
  }

  return (
    <div className={"pane why-paused"}>
      <div>
        <div>{L10N.getStr(reason)}</div>
        {renderMessage(why)}
      </div>
    </div>
  );
}
renderWhyPaused.displayName = "whyPaused";
