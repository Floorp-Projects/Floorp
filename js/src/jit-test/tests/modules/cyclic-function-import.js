// |jit-test| module

import { isOdd } from "isOdd.js"
import { isEven } from "isEven.js"

assertEq(isEven(4), true);
assertEq(isOdd(5), true);
