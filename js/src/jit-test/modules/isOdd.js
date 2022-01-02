import { isEven } from "isEven.js"

export function isOdd(x) {
    if (x < 0)
        throw "negative";
    if (x == 0)
        return false;
    return isEven(x - 1);
}

assertEq(isEven(4), true);
assertEq(isOdd(5), true);
