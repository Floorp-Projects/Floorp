import { assertEquals, assertRejects } from "@std/assert";
import { refreshLauncherForStoreMove } from "#libs/pwa/launcherUpdate.ts";

Deno.test("launcher refresh and store move succeed together", async () => {
  const events: string[] = [];
  const launcher = {
    install(manifest: string, strictIcon = false) {
      assertEquals(strictIcon, true);
      events.push(`install:${manifest}`);
      return Promise.resolve();
    },
  };
  const store = {
    moveSsbKey(oldKey: string, manifest: string) {
      events.push(`move:${oldKey}:${manifest}`);
      return Promise.resolve(true);
    },
  };

  assertEquals(
    await refreshLauncherForStoreMove(launcher, store, "old-key", "old", "new"),
    true,
  );
  assertEquals(events, ["install:new", "move:old-key:new"]);
});

Deno.test("launcher refresh rolls back when the store rejects the move", async () => {
  const installed: string[] = [];
  const launcher = {
    install(manifest: string) {
      installed.push(manifest);
      return Promise.resolve();
    },
  };
  const store = {
    moveSsbKey(_oldKey: string, _manifest: string) {
      return Promise.resolve(false);
    },
  };

  assertEquals(
    await refreshLauncherForStoreMove(launcher, store, "old-key", "old", "new"),
    false,
  );
  assertEquals(installed, ["new", "old"]);
});

Deno.test("launcher refresh restores the old launcher after a write error", async () => {
  const installed: string[] = [];
  const launcher = {
    install(manifest: string) {
      installed.push(manifest);
      return Promise.resolve();
    },
  };
  const store = {
    moveSsbKey(_oldKey: string, _manifest: string): Promise<boolean> {
      return Promise.reject(new Error("write failed"));
    },
  };

  await assertRejects(
    () => refreshLauncherForStoreMove(launcher, store, "old-key", "old", "new"),
    Error,
    "write failed",
  );
  assertEquals(installed, ["new", "old"]);
});

Deno.test("launcher refresh restores a partial install without moving the store", async () => {
  const installed: string[] = [];
  let moved = false;
  await assertRejects(
    () =>
      refreshLauncherForStoreMove(
        {
          install(manifest: string) {
            installed.push(manifest);
            return manifest === "new"
              ? Promise.reject(new Error("icon write failed"))
              : Promise.resolve();
          },
        },
        {
          moveSsbKey() {
            moved = true;
            return Promise.resolve(true);
          },
        },
        "old-key",
        "old",
        "new",
      ),
    Error,
    "icon write failed",
  );
  assertEquals(installed, ["new", "old"]);
  assertEquals(moved, false);
});

Deno.test("launcher refresh reports both the update and restoration failure", async () => {
  const updateError = new Error("update failed");
  const restoreError = new Error("restore failed");
  const error = await assertRejects(
    () =>
      refreshLauncherForStoreMove(
        {
          install(manifest: string) {
            return Promise.reject(
              manifest === "new" ? updateError : restoreError,
            );
          },
        },
        {
          moveSsbKey: () => Promise.resolve(true),
        },
        "old-key",
        "old",
        "new",
      ),
    AggregateError,
    "Failed to restore the original PWA launcher",
  );
  assertEquals(error.errors, [updateError, restoreError]);
});

Deno.test("launcher refresh reports restoration failure after a rejected move", async () => {
  await assertRejects(
    () =>
      refreshLauncherForStoreMove(
        {
          install(manifest: string) {
            return manifest === "old"
              ? Promise.reject(new Error("restore failed"))
              : Promise.resolve();
          },
        },
        {
          moveSsbKey: () => Promise.resolve(false),
        },
        "old-key",
        "old",
        "new",
      ),
    Error,
    "restore failed",
  );
});
