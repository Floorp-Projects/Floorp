/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

type Container = {
  userContextId: number;
  public: boolean;
  icon: string;
  color: number;
  name: string;
};

export function getUserContextColor(userContextId: number) {
  const container_list = ContextualIdentityService.getPublicIdentities();

  return (
    container_list.find(
      (container: Container) => container.userContextId === userContextId,
    )?.color ?? null
  );
}
