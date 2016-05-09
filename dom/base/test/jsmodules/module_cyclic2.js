import { func3 } from "./module_cyclic3.js";

export function func2(x, y) {
    if (x <= 0)
        return y;
    return func3(x - 1, y + "2");
}
