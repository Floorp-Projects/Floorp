/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This file should be loaded with `loadSubScript` because it contains
// stateful stores that need a new instance per test.

const NumberStore = {
  update: (state = 1, action, emitChange) => {
    switch(action.type) {
    case constants.ADD_NUMBER: {
      const newState = state + action.value;
      emitChange('number', newState);
      return newState;
    }
    case constants.DOUBLE_NUMBER: {
      const newState = state * 2;
      emitChange('number', newState);
      return newState;
    }}

    return state;
  },

  constants: {
    ADD_NUMBER: 'ADD_NUMBER',
    DOUBLE_NUMBER: 'DOUBLE_NUMBER'
  }
};

const itemsInitialState = {
  list: []
};
const ItemStore = {
  update: (state = itemsInitialState, action, emitChange) => {
    switch(action.type) {
    case constants.ADD_ITEM:
      state.list.push(action.item);
      emitChange('list', state.list);
      return state;
    }

    return state;
  },

  constants: {
    ADD_ITEM: 'ADD_ITEM'
  }
}
