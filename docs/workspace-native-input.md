# Workspace trackpad input

The browser's buttons and assignable shortcuts work on Windows, macOS, and
Linux. Direct three-finger input is a separate experimental feature. Its default
is off, and it requires both a matching native Runtime and verification of the
specific device and desktop environment.

## Native protocol

The native-only patch is
`.github/patches/floorp-runtime/common/workspace-trackpad-input.patch`, based on
the source commit recorded in `floorp-runtime.lock.json`. It adds a trusted,
chrome-only `MozWorkspaceSwipeGesture` event. Ordinary web content cannot observe
or consume this browser action. It does not reuse APZ's history-swipe stream.

| Property | Meaning |
| --- | --- |
| `fingerCount` | The actual native count. The GTK adapter starts only with three fingers. |
| `phase` | `start`, `update`, `end`, or `cancel`. Other existing gesture types use the empty string. |
| `operationId` | Nonzero ID shared by one stream, incremented per native window. |
| `delta` | Cumulative horizontal displacement in GTK logical pixels, positive to the right. |
| `deltaY` | Cumulative vertical displacement, positive downwards. |

The stream cancels if the finger count changes or a coordinate becomes invalid.
Updates and repeated ends after the stream finishes are ignored. The browser
controller decides horizontal intent and commits at most once, at the end.

Capability detection checks the compiled binding constant
`SimpleGestureEvent.prototype.WORKSPACE_SWIPE_PROTOCOL_VERSION === 1` together
with the native fields and platform. A preference alone is insufficient. The GTK
source build also sets `floorp.workspaces.gestures.native.gtk.version` to `1` as
diagnostic information. It is not the proof of a matching compiled Runtime.

Both `floorp.workspaces.gestures.native.enabled` and
`floorp.workspaces.gestures.native.verified` must be true for GTK to emit the
native stream. Both default to false. The second setting records a device and
environment verification; it must not be set by packaging or inferred from a
synthetic event test.

## OS support and verification state

No physical trackpad was available in the implementation environment. The table
therefore records source support and the remaining hardware checks separately.

| Environment | Direct source implementation | Verification / available alternative |
| --- | --- | --- |
| Linux / GTK 3.18+ / Wayland | Handles the genuine `GDK_TOUCHPAD_SWIPE` stream and `n_fingers`. | Unverified. The compositor must actually deliver three-finger events. Desktop-reserved gestures remain under OS control. |
| Linux / GTK / X11 | Uses the same GTK entry point if the device and driver emit it. | Unverified. Availability cannot be inferred from Wayland or a wheel event. |
| macOS | The checked `swipeWithEvent` route supplies direction, not a verified finger count. It is not converted into this protocol. | Direct input unavailable in this implementation. Buttons and shortcuts work; a user-configured gesture-to-key tool may send a shortcut where the OS permits it. |
| Windows | The checked Direct Manipulation path supplies pan phases and displacement, not the actual three-finger count. It is not relabelled as a three-finger stream. | Direct input unavailable in this implementation. Where Windows or a device driver exposes custom touchpad shortcuts, map one operation to one key chord. |

Use the separately assignable `floorp-workspace-next-no-wrap` and
`floorp-workspace-previous-no-wrap` actions for OS key mapping. They stop at the
first and last workspace. Existing next/previous actions retain their previous
wrapping behavior. Floorp cannot identify physical gesture boundaries in an
ordinary key stream, so the mapper must send one chord per gesture. Do not map
the same physical operation to both a key chord and native input.

Floorp does not change OS gesture settings. A desktop-reserved operation must
not be advertised as available to the browser merely because the adapter exists.

## Build and package boundary

The ordinary `package.yml` workflow performs an artifact build with precompiled
native libraries. It explicitly skips this native-only patch. Applying C++ and
WebIDL source changes there would not update the native libraries or bindings.
The patch is also absent from the local artifact patch directory.

The manual `workspace-native-input.yml` workflow checks out the exact locked
Runtime commit into a separate directory, validates this patch and applies all
common Floorp source patches in filename order, performs a full native build,
runs its GTests, and uploads an experimental native
package with the patch digest and source revision. This workflow does not
publish, update the canonical lock, or promote its artifact into the stable
release pipeline. Normal Runtime provenance and promotion checks still apply
before shipping a newly built native Runtime.

Validate the source and state machine without changing the sibling Runtime:

```sh
python3 tools/workspace-input/check-native-patch.py \
  --runtime-dir /path/to/pinned/Floorp-Runtime \
  --report /tmp/workspace-native-input-validation.json
```

The checker applies the patch in a temporary source fixture, validates the IDL
with the pinned Mozilla parser, and compiles and executes the actual native
stream state machine. `--apply` is intended only for a separate source-build
checkout. It is not an installation operation for a prebuilt Runtime.

After a full native build, run:

```sh
MOZ_HEADLESS=1 ./mach gtest 'WorkspaceSwipeGesture.*'
```

These automated checks do not establish physical device support. Before setting
the verified preference, record the OS version, GTK version, compositor/desktop,
Wayland or X11 session, trackpad/driver, reserved OS actions, and results for:

- a three-finger horizontal start/update/end, reverse movement, and cancellation;
- one switch per stream, with no repeated switch after release;
- two-finger scroll/history, vertical gestures, pinch, and tab dragging;
- blur, Escape, disabled workspaces, and native/sidebar input duplication;
- two windows with independent selections and reduced-motion/readout behavior.

The local validation report deliberately leaves `physicalDeviceVerified` false.
