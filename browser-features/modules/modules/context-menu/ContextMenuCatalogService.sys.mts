// SPDX-License-Identifier: MPL-2.0

import type {
  ContextMenuCatalogReporter,
  ContextMenuCatalogSnapshot,
  ContextMenuContainerDescriptor,
  ContextMenuSurfaceDescriptor,
} from "#features-chrome/common/context-menu/types.ts";

interface OwnerCatalogSnapshot {
  sequence: number;
  snapshot: ContextMenuCatalogSnapshot;
  containerSequences: Map<string, number>;
}

interface ContainerContribution {
  sequence: number;
  surfaceKey: string;
  profileKey: string;
  container: ContextMenuContainerDescriptor;
}

export interface ContextMenuCatalogServiceType
  extends ContextMenuCatalogReporter {
  getSnapshot(): ContextMenuCatalogSnapshot;
}

function cloneSerializable<T>(value: T): T {
  const serialized = JSON.stringify(value);
  if (serialized === undefined) {
    throw new TypeError("Context menu catalog snapshots must be serializable");
  }
  return JSON.parse(serialized) as T;
}

function mergeContainer(
  current: ContextMenuContainerDescriptor | undefined,
  incoming: ContextMenuContainerDescriptor,
): ContextMenuContainerDescriptor {
  // Every window initially reports placeholder containers. A placeholder from
  // a newly opened window must not erase a complete catalog captured in an
  // older window. A complete report is authoritative even when it is empty.
  if (!current || incoming.complete || !current.complete) {
    return incoming;
  }
  return current;
}

function containerSequenceKey(
  surfaceKey: string,
  profileKey: string,
  containerKey: string,
): string {
  return JSON.stringify([surfaceKey, profileKey, containerKey]);
}

function findContainer(
  snapshot: ContextMenuCatalogSnapshot,
  surfaceKey: string,
  profileKey: string,
  containerKey: string,
): ContextMenuContainerDescriptor | undefined {
  return snapshot.surfaces.find((surface) => surface.key === surfaceKey)
    ?.profiles.find((profile) => profile.key === profileKey)
    ?.containers.find((container) => container.key === containerKey);
}

function containersEqual(
  left: ContextMenuContainerDescriptor,
  right: ContextMenuContainerDescriptor,
): boolean {
  return JSON.stringify(left) === JSON.stringify(right);
}

function buildContainerSequences(
  snapshot: ContextMenuCatalogSnapshot,
  previous: OwnerCatalogSnapshot | undefined,
  reportSequence: number,
): Map<string, number> {
  const result = new Map<string, number>();
  for (const surface of snapshot.surfaces) {
    for (const profile of surface.profiles) {
      for (const container of profile.containers) {
        const key = containerSequenceKey(
          surface.key,
          profile.key,
          container.key,
        );
        const previousContainer = previous
          ? findContainer(
            previous.snapshot,
            surface.key,
            profile.key,
            container.key,
          )
          : undefined;
        const previousSequence = previous?.containerSequences.get(key);
        result.set(
          key,
          previousContainer && previousSequence !== undefined &&
            containersEqual(previousContainer, container)
            ? previousSequence
            : reportSequence,
        );
      }
    }
  }
  return result;
}

function collectContributions(
  owners: readonly OwnerCatalogSnapshot[],
): ContainerContribution[] {
  const result: ContainerContribution[] = [];
  for (const owner of owners) {
    for (const surface of owner.snapshot.surfaces) {
      for (const profile of surface.profiles) {
        for (const container of profile.containers) {
          const key = containerSequenceKey(
            surface.key,
            profile.key,
            container.key,
          );
          result.push({
            sequence: owner.containerSequences.get(key) ?? owner.sequence,
            surfaceKey: surface.key,
            profileKey: profile.key,
            container,
          });
        }
      }
    }
  }
  return result.sort((left, right) => left.sequence - right.sequence);
}

/**
 * Aggregates the catalogs reported by individual chrome-window controllers.
 *
 * A surface may be reported by more than one window. Owners are merged in the
 * order each profile/container was actually changed; complete observations
 * replace older values, while a new window's incomplete placeholders cannot
 * erase a previously observed container. All values are copied through JSON so
 * callers cannot retain mutable references and every returned snapshot can
 * cross a JSWindowActor boundary.
 */
export class ContextMenuCatalogStore implements ContextMenuCatalogServiceType {
  readonly #owners = new Map<string, OwnerCatalogSnapshot>();
  #revision = 0;
  #sequence = 0;

  report(ownerId: string, snapshot: ContextMenuCatalogSnapshot): void {
    if (ownerId.length === 0) {
      throw new TypeError("Context menu catalog ownerId must not be empty");
    }

    const clonedSnapshot = cloneSerializable(snapshot);
    const previous = this.#owners.get(ownerId);
    const sequence = ++this.#sequence;
    this.#owners.set(ownerId, {
      sequence,
      snapshot: clonedSnapshot,
      containerSequences: buildContainerSequences(
        clonedSnapshot,
        previous,
        sequence,
      ),
    });
    this.#revision++;
  }

  removeOwner(ownerId: string): void {
    if (this.#owners.delete(ownerId)) {
      this.#revision++;
    }
  }

  getSnapshot(): ContextMenuCatalogSnapshot {
    const owners = [...this.#owners.values()].sort((left, right) =>
      left.sequence - right.sequence
    );
    const surfaces = new Map<string, ContextMenuSurfaceDescriptor>();

    // Surface/profile metadata follows the latest owner report, as before.
    // Containers are merged separately using the sequence at which their
    // content actually changed.
    for (const owner of owners) {
      for (const surface of owner.snapshot.surfaces) {
        const currentSurface = surfaces.get(surface.key) ?? {
          key: surface.key,
          label: surface.label,
          profiles: [],
        };
        currentSurface.label = surface.label;
        for (const profile of surface.profiles) {
          const currentProfile = currentSurface.profiles.find((candidate) =>
            candidate.key === profile.key
          );
          if (currentProfile) currentProfile.label = profile.label;
          else {
            currentSurface.profiles.push({
              key: profile.key,
              label: profile.label,
              containers: [],
            });
          }
        }
        surfaces.set(surface.key, currentSurface);
      }
    }

    for (const contribution of collectContributions(owners)) {
      const surface = surfaces.get(contribution.surfaceKey);
      const profile = surface?.profiles.find((candidate) =>
        candidate.key === contribution.profileKey
      );
      if (!profile) continue;
      const containerIndex = profile.containers.findIndex((container) =>
        container.key === contribution.container.key
      );
      const merged = mergeContainer(
        containerIndex === -1 ? undefined : profile.containers[containerIndex],
        contribution.container,
      );
      if (containerIndex === -1) profile.containers.push(merged);
      else profile.containers[containerIndex] = merged;
    }

    const latestOwner = owners.at(-1);
    return cloneSerializable({
      schemaVersion: 1,
      revision: this.#revision,
      locale: latestOwner?.snapshot.locale ?? "",
      surfaces: [...surfaces.values()].sort((left, right) =>
        left.key.localeCompare(right.key)
      ),
    });
  }
}

export const ContextMenuCatalogService: ContextMenuCatalogServiceType =
  new ContextMenuCatalogStore();
