import { func1 } from "./module_cyclic1.js";

export function func3(x, y) {
    if (x <= 0)
        return y;
    return func1(x - 1, y + "3");
}
