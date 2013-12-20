function factorial(n) {
  if (n <= 1) {
    return 1;
  } else {
    return n * factorial(n - 1);
  }
}

function* yielder(n) {
  while (n-- >= 0) {
    yield { value: n, squared: n * n };
  }
}

function thrower() {
  throw new Error("Curse your sudden but inevitable betrayal!");
}

function main() {
  factorial(5);

  // XXX bug 923729: Can't test yielding yet.
  // for (let x of yielder(5)) {}

  try {
    thrower();
  } catch (e) {
  }
}
