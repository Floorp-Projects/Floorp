let holdings1 = [];
let holdings2 = [];
let holdings3 = [];
let holdings4 = [];
let holdings5 = [];

onmessage = (event) => {
  switch (event.data) {
  case 'startTest':
    startTest();
    break;
  case 'checkResults':
    checkResults();
    break;
  default:
    throw "Unknown message";
  }
};

function startTest() {
  // Registry with no registered objects.
  let registry1 = new FinalizationRegistry(i => holdings1 = [...i]);

  // Registry with three registered objects.
  let registry2 = new FinalizationRegistry(i => holdings2 = [...i]);
  registry2.register({}, 1);
  registry2.register({}, 2);
  registry2.register({}, 3);

  // Registry with registered object that is then unregistered.
  let registry3 = new FinalizationRegistry(i => holdings3 = [...i]);
  let token3 = {}
  registry3.register({}, 1, token3);
  registry3.unregister(token3);

  // Registry with registered object that doesn't die.
  let registry4 = new FinalizationRegistry(i => holdings4 = [...i]);
  let object4 = {};
  registry4.register(object4, 1);

  // Registry observing cyclic JS data structure.
  let registry5 = new FinalizationRegistry(i => holdings5 = [...i]);
  registry5.register(makeJSCycle(4), 5);

  const { gc } = getJSTestingFunctions();
  gc();

  Promise.resolve().then(() => {
    checkNoCallbacks();
  });

  postMessage('started');
}

function checkNoCallbacks() {
  is(holdings1.length, 0);
  is(holdings2.length, 0);
  is(holdings3.length, 0);
  is(holdings4.length, 0);
  is(holdings5.length, 0);
}

function checkResults() {
  is(holdings1.length, 0);

  let result = holdings2.sort((a, b) => a - b);
  is(result.length, 3);
  is(result[0], 1);
  is(result[1], 2);
  is(result[2], 3);

  is(holdings3.length, 0);
  is(holdings4.length, 0);

  is(holdings5.length, 1);
  is(holdings5[0], 5);

  postMessage('passed');
}

function is(a, b) {
  if (a !== b) {
    throw `Expected ${b} but got ${a}`;
  }
}

function makeJSCycle(size) {
  let first = {};
  let current = first;
  for (let i = 0; i < size; i++) {
    current.next = {};
    current = current.next;
  }
  current.next = first;
  return first;
}
