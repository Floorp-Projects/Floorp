export default function optimizedOut() {
  // Include enough logic to make Rollup not optimize the function out,
  // since we want to test the _engine_'s logic for optimizing it out.
  window.console;
}
