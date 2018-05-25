"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.fetchExtra = fetchExtra;
exports.getExtra = getExtra;

var _selectors = require("../../selectors/index");

var _preview = require("../../utils/preview");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
async function getReactProps(evaluate, displayName) {
  const componentNames = await evaluate(`
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
    `);
  const items = componentNames.result.preview && componentNames.result.preview.items;
  let extra = {
    displayName
  };

  if (items) {
    extra = {
      displayName,
      componentStack: items
    };
  }

  return extra;
}

async function getImmutableProps(expression, evaluate) {
  const immutableEntries = await evaluate((exp => `${exp}.toJS()`)(expression));
  const immutableType = await evaluate((exp => `${exp}.constructor.name`)(expression));
  return {
    type: immutableType.result,
    entries: immutableEntries.result
  };
}

async function getExtraProps(getState, expression, result, evaluate) {
  const props = {};
  const component = (0, _selectors.inComponent)(getState());

  if (component) {
    props.react = await getReactProps(evaluate, component);
  }

  if ((0, _preview.isImmutablePreview)(result)) {
    props.immutable = await getImmutableProps(expression, evaluate);
  }

  return props;
}

function fetchExtra() {
  return async function ({
    dispatch,
    getState
  }) {
    const frame = (0, _selectors.getSelectedFrame)(getState());
    const extra = await dispatch(getExtra("this;", frame.this));
    dispatch({
      type: "ADD_EXTRA",
      extra: extra
    });
  };
}

function getExtra(expression, result) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const selectedFrame = (0, _selectors.getSelectedFrame)(getState());

    if (!selectedFrame) {
      return;
    }

    const extra = await getExtraProps(getState, expression, result, expr => client.evaluateInFrame(expr, selectedFrame.id));
    return extra;
  };
}