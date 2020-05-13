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
  let registry1 = new FinalizationRegistry(v => { holdings1.push(v); });

  // Registry with three registered objects.
  let registry2 = new FinalizationRegistry(v => { holdings2.push(v); });
  registry2.register({}, 1);
  registry2.register({}, 2);
  registry2.register({}, 3);

  // Registry with registered object that is then unregistered.
  let registry3 = new FinalizationRegistry(v => { holdings3.push(v); });
  let token3 = {}
  registry3.register({}, 1, token3);
  registry3.unregister(token3);

  // Registry with registered object that doesn't die.
  let registry4 = new FinalizationRegistry(v => { holdings4.push(v); });
  let object4 = {};
  registry4.register(object4, 1);

  // Registry observing cyclic JS data structure.
  let registry5 = new FinalizationRegistry(v => { holdings5.push(v); });
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
