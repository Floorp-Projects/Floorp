/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

loadSubScript(getFileUrl('stores-for-testing.js'));
const constants = NumberStore.constants;
const stores = { number: NumberStore };

function run_test() {
  run_next_test();
}

add_task(function* testThunkDispatch() {
  do_print("Testing thunk dispatch");
  // The thunk middleware allows you to return a function from an
  // action creator which takes `dispatch` and `getState` functions as
  // arguments. This allows the action creator to fire multiple
  // actions manually with `dispatch`, possibly asynchronously.

  function addNumberLater(num) {
    return dispatch => {
      // Just do it in the next tick, no need to wait too long
      waitForTick().then(() => {
        dispatch({
          type: constants.ADD_NUMBER,
          value: num
        });
      });
    };
  }

  function addNumber(num) {
    return (dispatch, getState) => {
      dispatch({
        type: constants.ADD_NUMBER,
        value: getState().number > 10 ? (num * 2) : num
      });
    };
  }

  const dispatcher = createDispatcher(stores);
  equal(dispatcher.getState().number, 1);
  dispatcher.dispatch(addNumberLater(5));
  equal(dispatcher.getState().number, 1, "state should not have changed");
  yield waitForTick();
  equal(dispatcher.getState().number, 6, "state should have changed");

  dispatcher.dispatch(addNumber(5));
  equal(dispatcher.getState().number, 11);
  dispatcher.dispatch(addNumber(2));
  // 2 * 2 should have actually been added because the state is
  // greater than 10 (the action creator changes the value based on
  // the current state)
  equal(dispatcher.getState().number, 15);
});

add_task(function* testWaitUntilService() {
  do_print("Testing waitUntil service");
  // The waitUntil service allows you to queue functions to be run at a
  // later time, depending on a predicate. As actions comes through
  // the system, you predicate will be called with each action. Once
  // your predicate returns true, the queued function will be run and
  // removed from the pending queue.

  function addWhenDoubled(num) {
    return {
      type: services.WAIT_UNTIL,
      predicate: action => action.type === constants.DOUBLE_NUMBER,
      run: (dispatch, getState, action) => {
        ok(action.type, constants.DOUBLE_NUMBER);
        ok(getState(), 10);

        dispatch({
          type: constants.ADD_NUMBER,
          value: 2
        });
      }
    };
  }

  function addWhenGreaterThan(threshold, num) {
    return (dispatch, getState) => {
      dispatch({
        type: services.WAIT_UNTIL,
        predicate: () => getState().number > threshold,
        run: () => {
          dispatch({
            type: constants.ADD_NUMBER,
            value: num
          });
        }
      });
    }
  }

  const dispatcher = createDispatcher(stores);

  // Add a pending action that adds 2 after the number is doubled
  equal(dispatcher.getState().number, 1);
  dispatcher.dispatch(addWhenDoubled(2));
  equal(dispatcher.getState().number, 1);
  dispatcher.dispatch({ type: constants.DOUBLE_NUMBER });
  // Note how the pending function we added ran synchronously. It
  // should have added 2 after doubling 1, so 1 * 2 + 2 = 4
  equal(dispatcher.getState().number, 4);

  // Add a pending action that adds 5 once the number is greater than 10
  dispatcher.dispatch(addWhenGreaterThan(10, 5));
  equal(dispatcher.getState().number, 4);
  dispatcher.dispatch({ type: constants.ADD_NUMBER, value: 10 });
  // Again, the pending function we added ran synchronously. It should
  // have added 5 more after 10 was added, since the number was
  // greater than 10.
  equal(dispatcher.getState().number, 19);
});
