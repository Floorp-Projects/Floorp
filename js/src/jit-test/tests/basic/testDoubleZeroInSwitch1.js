var a = Math.sin(Math.PI/2);  // spec does not specify precise answer here...
if (a === 1) {    // ...but if a === 1 here...
    switch (a) {
      case 1: break;  // ...then it must also match here
      default: throw "FAIL";
    }
}
