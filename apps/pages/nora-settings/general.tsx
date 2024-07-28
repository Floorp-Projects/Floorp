import "./general.css";

import IcnCheck from "~icons/mdi/check";

const Checkbox = (props) => {
  return (
    <label class="checkboxRoot">
      {props.children}
      <input type="checkbox" class="checkbox" />
      <div class="switch">
        <div class="switch-dot"></div>
      </div>
    </label>
  );
};

export function App() {
  return (
    <main>
      <Checkbox>test</Checkbox>
    </main>
  );
}
