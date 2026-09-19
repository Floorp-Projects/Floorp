# Panel layout regressions

`panelSidebarLayout.test.ts` runs inside Floorp and uses the actual sidebar DOM
and embedded browser. It covers:

- #2761: the child viewport fits 225, 400, and 600px panels despite the normal
  taskbar window's 804px minimum width.
- #2757: the native hover launcher's outer edge stays in place for both Firefox
  positions and both Floorp positions, with the panel open and closed, during
  expansion and collapse animation.
- #2722: completed docked resizing updates preferences and survives closing and
  recreating a panel; floating resizing saves its final queued animation frame.
  A floating panel does not contribute to the native hover launcher's offset.

Run with the browser test runner:

```sh
deno task test --near browser-features/chrome/common/panel-sidebar/test/panelSidebarLayout.test.ts
deno task test --near browser-features/chrome/common/flex-order
```

Use an isolated test profile. The standard runner uses shared development ports;
serialize browser runs if another worktree is using those ports.

For an account-free manual stress check, serve `fixtures/responsive-panel.html`
on a loopback port and add its URL as a web panel. It contains 2,400 grid cards
and reports viewport width and resize count in its title. Drag the native
splitter repeatedly between 400 and 540px on both sides, close/reopen the panel,
and restart the test profile. With vertical tabs set to expand on hover, hold
the pointer over the collapsed launcher for at least two seconds and verify that
its outer edge remains fixed. Repeat with the web panel closed.

This fixture exercises responsive layout and width persistence. Passing it does
not establish that the WhatsApp-specific unresponsive state in #2722 is fixed.
