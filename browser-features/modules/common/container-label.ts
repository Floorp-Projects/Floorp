// SPDX-License-Identifier: MPL-2.0

/** The part of a ContextualIdentityService public identity needed for its label. */
export type ContainerLabelIdentity = {
  userContextId: number;
  name?: string;
};

/**
 * Returns the display label of a container.
 *
 * Gecko no longer stores `name` or `l10nId` on the built-in containers
 * (ContextualIdentityService migrate7to8), so callers must not gate the
 * lookup on `l10nId`. `getUserContextLabel` resolves both built-in (Fluent)
 * and user-created names; the stored name is only a fallback. Returns "" when
 * neither is available.
 */
export function resolveContainerLabel(
  identity: ContainerLabelIdentity,
  getUserContextLabel: (userContextId: number) => string,
): string {
  return getUserContextLabel(identity.userContextId) || identity.name || "";
}
