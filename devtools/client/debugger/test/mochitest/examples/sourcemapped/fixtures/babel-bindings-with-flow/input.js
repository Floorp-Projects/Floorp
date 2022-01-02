// Webpack doesn't map import declarations well, so doing this forces binding
// processing to only process the binding being cast when searching for
// matches. That way we can properly test if the flow cast causes problems.
import { aNamed } from "./src/mod";

export default function root() {
  var value = (aNamed: Array<string>);

  console.log("pause here", root, value);
}
