import { createSignal } from "solid-js";

export function Counter() {
  const [count, setCount] = createSignal(0);
  setInterval(() => setCount(count() + 1), 1000);
  return (
    <>
      <div
        style="font-size:30px"
        onClick={() => {
          window.alert("click!");
        }}
      >
        Count aa hmr: {count()}
      </div>
    </>
  );
}
