// SPDX-License-Identifier: MPL-2.0

import { render, h } from "preact";
import { Counter } from "./counter";

render(h(Counter, null), document.getElementById("browser")!);
