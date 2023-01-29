# Storage Panel Architecture

## Actor structure

This is the new architecture that is being implemented to support Fission. It's currently used when inspecting tabs.

![Class structure architecture (resource-based)](storage/resources.svg)

- We no longer have a global `Storage` actor.
- The specific actors for each storage type are spawned by watchers instead.
- The reference to a global `Storage` actor that each actor has now points to a mock instead.
- Some watchers require to be run in the parent process, while others can be run in the content process.
  - Parent process: Cookies, IndexedDB, Web Extension.
  - Content process: LocalStorage, SessionStorage, Cache.

## Flow

Some considerations to keep in mind:

- In the Storage Panel, **resources are fronts**.
- These fronts contain a `hosts` object, which is populated with the host name, and the actual storage data it contains.
- In the client, we get as part of the `onAvailable` callback of `ResourceCommand.watchResources`:
  - Content process storage types: multiple resources, one per target
  - Parent process storage types: a single resource

### Initial load

Web page loaded, open toolbox. Later on, we see what happens if a new remote target is added (for instance, an iframe is created that points to a different host).

#### Fission OFF

![Initial load diagram, fission off](storage/flow-fission-off.svg)

- We get all the storage fronts as new resources sent in the `onAvailable` callback for `watchResources`.
- After a remote target has been added, we get new additions as `"single-store-update"` events.

#### Fission ON

![Initial load diagram, fission on](storage/flow-fission-on.svg)

Similar to the previous scenario (fission off), but now when a new remote target is added:

- We get content process storage resources in a new `onAvailable` callback, instead of `"single-store-update"`.
- Parent process storage resources keep using the `"single-store-update"` method. This is possible due to their `StorageMock` actors emitting a fake `"window-ready"` event after a `"window-global-created"`.

### Navigation

#### Fission ON, target switching OFF

![Navigation diagram, fission on, target switching off](storage/navigation-fission-on-target-switching-off.svg)

- Deletion of content process storage hosts is handled within the `onTargetDestroyed` callback.
- Deletion of parent process storage hosts is handled with `"single-store-update"` events, fired when the `StorageMock` detects a `"window-global-destroyed"` event.
- When the new target is available, new storage actors are spawned from their watchers' `watch` method and are sent as resources in the `onAvailable` callback.

#### Fission ON, target switching ON

![Navigation diagram, fission on, target switching off](storage/navigation-fission-on-target-switching-on.svg)

Similar to the previous scenario (fission on, target switching off), but parent process storage resources are handled differently, since their watchers remain instantiated.

- New actors for parent process resources are not spawned by their watchers `watch`, but as a callback of `"window-global-created"`.
- Some times there's a race condition between a target being available and firing `"window-global-created"`. There is a delay to send the resource to the client, to ensure that any `onTargetAvailable` callback is processed first.
  - The new actor/resource is sent after a `"target-available-form"` event.

### CRUD operations

#### Add a new cookie

Other CRUD operations work very similar to this one.

![CRUD operation diagram, add a new cookie](storage/crud-cookie.svg)

- We call `StorageMock.getWindowFromHost` so we can get the storage principal. Since this is a parent process resource, it doesn't have access to an actual window, so it returns a mock instead (but with a real principal).
- To detect changes in storage, we subscribe to different events that platform provides via `Services.obs.addObserver`.
- To manipulate storage data, we use different methods depending on the storage type. For cookies, we use the API provided by `Services.cookies`.
