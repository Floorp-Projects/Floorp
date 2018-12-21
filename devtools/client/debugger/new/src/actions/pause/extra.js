/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getCurrentThread,
  getSource,
  inComponent,
  getSelectedFrame
} from "../../selectors";
import { isImmutablePreview } from "../../utils/preview";

import type { ThunkArgs } from "../types";

async function getReactProps(evaluate, displayName) {
  const componentNames = await evaluate(
    `
    if(this.hasOwnProperty('_reactInternalFiber')) {
      let componentNames = [];
      let componentNode = this._reactInternalFiber;
      while(componentNode) {
        componentNames.push(componentNode.type.name);
        componentNode = componentNode._debugOwner
      }
      componentNames;
    }
    else {
      [this._reactInternalInstance.getName()];
    }
    `
  );

  const items =
    componentNames.result.preview && componentNames.result.preview.items;

  let extra = { displayName };
  if (items) {
    extra = { displayName, componentStack: items };
  }

  return extra;
}

async function getImmutableProps(expression: string, evaluate) {
  // NOTE: it's possible the expression is a statement e.g `_this.fields;`
  expression = expression.replace(/;$/, "");

  const immutableEntries = await evaluate(`${expression}.toJS()`);
  const immutableType = await evaluate(`${expression}.constructor.name`);

  return {
    type: immutableType.result,
    entries: immutableEntries.result
  };
}

async function getExtraProps(getState, expression, result, evaluate) {
  const props = {};

  const component = inComponent(getState());

  if (component) {
    props.react = await getReactProps(evaluate, component);
  }

  if (isImmutablePreview(result)) {
    props.immutable = await getImmutableProps(expression, evaluate);
  }

  return props;
}

export function fetchExtra() {
  return async function({ dispatch, getState }: ThunkArgs) {
    const frame = getSelectedFrame(getState());
    if (!frame) {
      return;
    }

    const extra = await dispatch(getExtra("this;", frame.this));
    dispatch({
      type: "ADD_EXTRA",
      thread: getCurrentThread(getState()),
      extra: extra
    });
  };
}

export function getExtra(expression: string, result: Object) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const selectedFrame = getSelectedFrame(getState());
    if (!selectedFrame) {
      return {};
    }

    const source = getSource(getState(), selectedFrame.location.sourceId);
    if (!source) {
      return {};
    }

    const extra = await getExtraProps(getState, expression, result, expr =>
      client.evaluateInFrame(expr, {
        frameId: selectedFrame.id,
        thread: source.thread
      })
    );

    return extra;
  };
}
