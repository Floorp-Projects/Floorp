"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.fetchExtra = fetchExtra;
exports.getExtra = getExtra;

var _selectors = require("../../selectors/index");

var _preview = require("../../utils/preview");

var _ast = require("../../utils/ast");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

async function getReactProps(evaluate) {
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

  if (items) {
    return {
      displayName: items[0],
      componentStack: items
    };
  }
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

  if ((0, _preview.isReactComponent)(result)) {
    const selectedFrame = (0, _selectors.getSelectedFrame)(getState());
    const source = (0, _selectors.getSource)(getState(), selectedFrame.location.sourceId);
    const symbols = (0, _selectors.getSymbols)(getState(), source);

    if (symbols && symbols.classes) {
      const originalClass = (0, _ast.findClosestClass)(symbols, selectedFrame.location);

      if (originalClass) {
        props.react = {
          displayName: originalClass.name
        };
      }
    }

    props.react = _objectSpread({}, (await getReactProps(evaluate)), props.react);
  }

  if ((0, _preview.isImmutable)(result)) {
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