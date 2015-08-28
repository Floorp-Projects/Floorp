/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

loadSubScript(getFileUrl("stores-for-testing.js"));
const constants = Object.assign(
  {},
  NumberStore.constants,
  ItemStore.constants
);
const stores = { number: NumberStore,
                 items: ItemStore };

function addNumber(num) {
  return {
    type: constants.ADD_NUMBER,
    value: num
  }
}

function addItem(item) {
  return {
    type: constants.ADD_ITEM,
    item: item
  };
}

// Tests

function run_test() {
  testInitialValue();
  testDispatch();
  testEmitChange();
  run_next_test();
}

function testInitialValue() {
  do_print("Testing initial value");
  const dispatcher = createDispatcher(stores);
  equal(dispatcher.getState().number, 1);
}

function testDispatch() {
  do_print("Testing dispatch");

  const dispatcher = createDispatcher(stores);
  dispatcher.dispatch(addNumber(5));
  equal(dispatcher.getState().number, 6);

  dispatcher.dispatch(addNumber(2));
  equal(dispatcher.getState().number, 8);

  // It should ignore unknown action types
  dispatcher.dispatch({ type: "FOO" });
  equal(dispatcher.getState().number, 8);
}

function testEmitChange() {
  do_print("Testing change emittters");
  const dispatcher = createDispatcher(stores);
  let listenerRan = false;

  const numberView = {
    x: 3,
    renderNumber: function(num) {
      ok(this.x, 3, "listener ran in context of view");
      ok(num, 10);
      listenerRan = true;
    }
  }

  // Views can listen to changes in state by specifying which part of
  // the state to listen to and giving a handler function. The
  // function will be run with the view as `this`.
  dispatcher.onChange({
    "number": numberView.renderNumber
  }, numberView);

  dispatcher.dispatch(addNumber(9));
  ok(listenerRan, "number listener actually ran");
  listenerRan = false;

  const itemsView = {
    renderList: function(items) {
      ok(items.length, 1);
      ok(items[0].name = "james");
      listenerRan = true;
    }
  }

  // You can listen to deeper sections of the state by nesting objects
  // to specify the path to that state. You can do this 1 level deep;
  // you cannot arbitrarily nest state listeners.
  dispatcher.onChange({
    "items": {
      "list": itemsView.renderList
    }
  }, itemsView);

  dispatcher.dispatch(addItem({ name: "james" }));
  ok(listenerRan, "items listener actually ran");
}
