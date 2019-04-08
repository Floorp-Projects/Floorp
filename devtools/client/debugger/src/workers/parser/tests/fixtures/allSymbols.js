const TIME = 60;
let count = 0;

function incrementCounter(counter) {
  return counter++;
}

const sum = (a, b) => a + b;

const Obj = {
  foo: 1,
  doThing() {
    console.log("hey");
  },
  doOtherThing: function() {
    return 42;
  }
};

Obj.property = () => {};
Obj.otherProperty = 1;

class Ultra {
  constructor() {
    this.awesome = true;
  }

  beAwesome(person) {
    console.log(`${person} is Awesome!`);
  }
}

this.props.history.push(`/dacs/${this.props.dac.id}`);
