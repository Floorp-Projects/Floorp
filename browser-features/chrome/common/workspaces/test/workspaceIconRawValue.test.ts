// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  applyWorkspaceIconSelection,
  isCanonicalWorkspaceIconId,
  WorkspaceIcons,
} from "../utils/workspace-icons.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import {
  applyWorkspaceModalResult,
  createWorkspaceIconPickerFormItem,
} from "../workspace-modal.tsx";
import { WorkspacesService } from "../workspacesService.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const makeWorkspace = (): Omit<TWorkspace, "icon"> => ({
  name: "Raw icon",
  userContextId: 0,
  isSelected: null,
  isDefault: null,
});

const assertSameRawIconSlot = (
  expected: TWorkspace,
  actual: TWorkspace,
  label: string,
): void => {
  assertEquals(
    Object.hasOwn(actual, "icon"),
    Object.hasOwn(expected, "icon"),
    `${label} presence`,
  );
  if (Object.hasOwn(expected, "icon")) {
    assertEquals(actual.icon, expected.icon, `${label} value`);
  }
};

function testNoChangePreservesEveryRawCategory(): void {
  const cases: Array<[string, TWorkspace]> = [
    ["absent", makeWorkspace()],
    ["own undefined", { ...makeWorkspace(), icon: undefined }],
    ["null", { ...makeWorkspace(), icon: null }],
    ["alias", { ...makeWorkspace(), icon: "article" }],
    ["canonical", { ...makeWorkspace(), icon: "floorp-icon:v1:article" }],
    ["opaque", { ...makeWorkspace(), icon: "future:workspace-icon" }],
    ["URI", { ...makeWorkspace(), icon: "https://example.invalid/icon.svg" }],
    ["raw data URI", { ...makeWorkspace(), icon: "data:image/svg+xml,<svg/>" }],
  ];
  for (const [label, workspace] of cases) {
    const unchanged = applyWorkspaceIconSelection(
      workspace,
      "__floorp_workspace_icon_picker_no_change__",
    );
    assertSameRawIconSlot(workspace, unchanged, label);
  }
}

function testInvalidPickerResultsPreserveRawSlot(): void {
  const workspace: TWorkspace = { ...makeWorkspace(), icon: "article" };
  for (
    const candidate of [
      null,
      undefined,
      "article",
      "floorp-icon:v1:unknown",
      "https://example.invalid/icon.svg",
    ]
  ) {
    assertSameRawIconSlot(
      workspace,
      applyWorkspaceIconSelection(workspace, candidate),
      `invalid ${String(candidate)}`,
    );
  }
}

function testExplicitCanonicalSelectionOverwritesEveryCategory(): void {
  const selected = "floorp-icon:v1:music";
  assert(isCanonicalWorkspaceIconId(selected), "selection is canonical");
  const cases: TWorkspace[] = [
    makeWorkspace(),
    { ...makeWorkspace(), icon: undefined },
    { ...makeWorkspace(), icon: null },
    { ...makeWorkspace(), icon: "article" },
    { ...makeWorkspace(), icon: "floorp-icon:v1:article" },
    { ...makeWorkspace(), icon: "opaque" },
  ];
  for (const workspace of cases) {
    const changed = applyWorkspaceIconSelection(workspace, selected);
    assert(
      Object.hasOwn(changed, "icon"),
      "explicit selection creates icon slot",
    );
    assertEquals(
      changed.icon,
      selected,
      "explicit selection stores canonical ID",
    );
  }
}

function testJsonCategoriesDoNotCollapseToNull(): void {
  const absent = JSON.parse(JSON.stringify(makeWorkspace())) as Record<
    string,
    unknown
  >;
  const ownUndefined = JSON.parse(
    JSON.stringify({ ...makeWorkspace(), icon: undefined }),
  ) as Record<string, unknown>;
  const explicitNull = JSON.parse(
    JSON.stringify({ ...makeWorkspace(), icon: null }),
  ) as Record<string, unknown>;
  const opaque = JSON.parse(
    JSON.stringify({ ...makeWorkspace(), icon: "future:value" }),
  ) as Record<string, unknown>;
  assert(!Object.hasOwn(absent, "icon"), "absent icon stays omitted in JSON");
  assert(
    !Object.hasOwn(ownUndefined, "icon"),
    "own undefined becomes absent in JSON, not null",
  );
  assertEquals(explicitNull.icon, null, "explicit null stays null in JSON");
  assertEquals(
    opaque.icon,
    "future:value",
    "opaque strings round-trip exactly",
  );
}

function testModalFormNeverTransportsRawStoredValue(): void {
  const icons = new WorkspaceIcons();
  const base = makeWorkspace();
  const cases: Array<[string, TWorkspace, string]> = [
    ["absent", base, "floorp-icon:v1:fingerprint"],
    [
      "own undefined",
      { ...base, icon: undefined },
      "floorp-icon:v1:fingerprint",
    ],
    ["null", { ...base, icon: null }, "floorp-icon:v1:fingerprint"],
    ["alias", { ...base, icon: "article" }, "floorp-icon:v1:article"],
    [
      "canonical",
      { ...base, icon: "floorp-icon:v1:article" },
      "floorp-icon:v1:article",
    ],
    ["opaque", { ...base, icon: "future:value" }, "floorp-icon:v1:fingerprint"],
    [
      "URI",
      { ...base, icon: "https://example.invalid/raw.svg" },
      "floorp-icon:v1:fingerprint",
    ],
  ];
  for (const [label, workspace, expectedDisplay] of cases) {
    const field = createWorkspaceIconPickerFormItem(
      workspace,
      icons,
      (alias) => `label:${alias}`,
      "Icon",
    );
    assertEquals(
      field.type,
      "workspace-icon-picker",
      `${label} dedicated type`,
    );
    assertEquals(field.displayValue, expectedDisplay, `${label} safe display`);
    assertEquals(field.options?.length, 34, `${label} has exactly 34 options`);
    assert(
      field.options?.every((option) =>
        option.value.startsWith("floorp-icon:v1:") &&
        option.icon?.startsWith("data:image/svg+xml;base64,")
      ),
      `${label} options are canonical with bundled previews`,
    );
    if (typeof workspace.icon === "string") {
      assert(
        field.value !== workspace.icon,
        `${label} raw string is not the submitted initial value`,
      );
    }
    if (workspace.icon?.includes("://")) {
      assert(
        !JSON.stringify(field).includes(workspace.icon),
        `${label} raw URI is not transported anywhere`,
      );
    }
  }
}

function testModalResultTransportCoversCancelNoChangeInvalidAndSelection(): void {
  const rawCases: TWorkspace[] = [
    makeWorkspace(),
    { ...makeWorkspace(), icon: undefined },
    { ...makeWorkspace(), icon: null },
    { ...makeWorkspace(), icon: "article" },
    { ...makeWorkspace(), icon: "floorp-icon:v1:article" },
    { ...makeWorkspace(), icon: "future:value" },
    { ...makeWorkspace(), icon: "https://example.invalid/icon.svg" },
    { ...makeWorkspace(), icon: "data:image/svg+xml,<svg/>" },
  ];
  for (const workspace of rawCases) {
    assertEquals(
      applyWorkspaceModalResult(workspace, null),
      null,
      "cancel produces no workspace update",
    );

    const noChange = applyWorkspaceModalResult(workspace, {
      name: "Edited",
      userContextId: "4",
      icon: "__floorp_workspace_icon_picker_no_change__",
    });
    assert(noChange !== null, "no-change submit returns edited workspace");
    assertSameRawIconSlot(workspace, noChange, "no-change submit");
    assertEquals(noChange.name, "Edited", "no-change still applies name edit");
    assertEquals(noChange.userContextId, 4, "no-change applies container edit");

    for (
      const invalid of [
        "article",
        "floorp-icon:v1:unknown",
        "https://example.invalid/icon.svg",
        42,
      ]
    ) {
      const invalidResult = applyWorkspaceModalResult(workspace, {
        name: workspace.name,
        userContextId: workspace.userContextId,
        icon: invalid,
      });
      assert(invalidResult !== null, "invalid submit returns workspace");
      assertSameRawIconSlot(
        workspace,
        invalidResult,
        `invalid ${String(invalid)}`,
      );
    }

    const selected = applyWorkspaceModalResult(workspace, {
      name: workspace.name,
      userContextId: workspace.userContextId,
      icon: "floorp-icon:v1:music",
    });
    assert(selected !== null, "explicit selection returns workspace");
    assertEquals(
      selected.icon,
      "floorp-icon:v1:music",
      "explicit selection stores canonical ID",
    );
  }
}

type RawServiceCase = {
  label: string;
  hasOwn: boolean;
  value?: TWorkspace["icon"];
  persistedHasOwn: boolean;
  persistedValue?: TWorkspace["icon"];
};

type ConcurrentWorkspace = TWorkspace & {
  concurrentMarker?: string;
};

const createCollisionCheckedTestID = (
  data: ReadonlyMap<TWorkspaceID, TWorkspace>,
  order: readonly TWorkspaceID[],
): TWorkspaceID => {
  for (let counter = 1; counter <= 999; counter++) {
    const candidate = `00000000-0000-4000-8000-${
      counter.toString().padStart(12, "0")
    }` as TWorkspaceID;
    if (!data.has(candidate) && !order.includes(candidate)) {
      return candidate;
    }
  }
  throw new Error("Unable to allocate a collision-free workspace test UUID");
};

const cloneLiveWorkspaceMap = (): Map<TWorkspaceID, TWorkspace> =>
  new Map(
    workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>,
  );

const replaceWorkspaceStore = (
  data: Map<TWorkspaceID, TWorkspace>,
  order: readonly TWorkspaceID[],
  defaultID: TWorkspaceID,
): void => {
  setWorkspacesDataStore({
    data: data as unknown as TWorkspacesStoreData["data"],
    order: [...order] as TWorkspacesStoreData["order"],
    defaultID,
  });
};

const setRawIconSlot = (
  workspace: ConcurrentWorkspace,
  rawCase: Pick<RawServiceCase, "hasOwn" | "value">,
): ConcurrentWorkspace => {
  const next: ConcurrentWorkspace = { ...workspace };
  if (rawCase.hasOwn) {
    next.icon = rawCase.value;
  } else {
    delete next.icon;
  }
  return next;
};

const flushWorkspacePersistence = async (): Promise<void> => {
  await Promise.resolve();
  await Promise.resolve();
};

async function testServiceUsesLatestWorkspaceWithoutResurrection(): Promise<
  void
> {
  const originalData = cloneLiveWorkspaceMap();
  const originalOrder = [...workspacesDataStore.order] as TWorkspaceID[];
  const originalDefaultID = workspacesDataStore.defaultID as TWorkspaceID;
  const hadWorkspacePref = Services.prefs.prefHasUserValue(
    WORKSPACE_DATA_PREF_NAME,
  );
  const originalWorkspacePref = hadWorkspacePref
    ? Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME)
    : null;
  const targetID = createCollisionCheckedTestID(originalData, originalOrder);
  const testOrder = [...originalOrder, targetID];
  let visibilityUpdates = 0;
  let onShow: () => Promise<Record<string, unknown> | null> = () =>
    Promise.resolve(null);
  const service = Object.create(
    WorkspacesService.prototype,
  ) as WorkspacesService;
  service.dataManagerCtx = {
    getRawWorkspace: (id: TWorkspaceID) => cloneLiveWorkspaceMap().get(id),
  } as WorkspacesService["dataManagerCtx"];
  service.modalCtx = {
    showWorkspacesModal: async () => await onShow(),
  } as unknown as WorkspacesService["modalCtx"];
  service.tabManagerCtx = {
    updateTabsVisibility: () => {
      visibilityUpdates++;
    },
  } as unknown as WorkspacesService["tabManagerCtx"];

  const rawCases: RawServiceCase[] = [
    { label: "absent", hasOwn: false, persistedHasOwn: false },
    {
      label: "own undefined",
      hasOwn: true,
      value: undefined,
      persistedHasOwn: false,
    },
    {
      label: "null",
      hasOwn: true,
      value: null,
      persistedHasOwn: true,
      persistedValue: null,
    },
    {
      label: "alias",
      hasOwn: true,
      value: "article",
      persistedHasOwn: true,
      persistedValue: "article",
    },
    {
      label: "canonical",
      hasOwn: true,
      value: "floorp-icon:v1:article",
      persistedHasOwn: true,
      persistedValue: "floorp-icon:v1:article",
    },
    {
      label: "opaque",
      hasOwn: true,
      value: "future:concurrent-icon",
      persistedHasOwn: true,
      persistedValue: "future:concurrent-icon",
    },
    {
      label: "URI",
      hasOwn: true,
      value: "https://example.invalid/concurrent.svg",
      persistedHasOwn: true,
      persistedValue: "https://example.invalid/concurrent.svg",
    },
    {
      label: "raw data URI",
      hasOwn: true,
      value: "data:image/svg+xml,<svg id='concurrent'/>",
      persistedHasOwn: true,
      persistedValue: "data:image/svg+xml,<svg id='concurrent'/>",
    },
  ];

  const installOpenWorkspace = (): void => {
    const data = cloneLiveWorkspaceMap();
    data.set(targetID, {
      ...makeWorkspace(),
      name: "Open-time workspace",
      icon: "floorp-icon:v1:book",
    });
    replaceWorkspaceStore(data, testOrder, originalDefaultID);
  };

  try {
    for (const rawCase of rawCases) {
      installOpenWorkspace();
      visibilityUpdates = 0;
      onShow = async () => {
        const data = cloneLiveWorkspaceMap();
        const current = data.get(targetID);
        assert(current, `${rawCase.label}: test workspace disappeared`);
        const concurrent = setRawIconSlot(
          {
            ...current,
            isSelected: true,
            isDefault: false,
            concurrentMarker: `latest:${rawCase.label}`,
          },
          rawCase,
        );
        data.set(targetID, concurrent);
        replaceWorkspaceStore(data, testOrder, originalDefaultID);
        await flushWorkspacePersistence();
        return {
          name: `Submitted ${rawCase.label}`,
          userContextId: "7",
          icon: "__floorp_workspace_icon_picker_no_change__",
        };
      };

      const result = await service.manageWorkspaceFromDialog(targetID);
      assert(result, `${rawCase.label}: modal result was not returned`);
      await flushWorkspacePersistence();
      const actual = cloneLiveWorkspaceMap().get(targetID) as
        | ConcurrentWorkspace
        | undefined;
      assert(actual, `${rawCase.label}: workspace was not retained`);
      assertEquals(
        actual.name,
        `Submitted ${rawCase.label}`,
        `${rawCase.label}: submitted name`,
      );
      assertEquals(actual.userContextId, 7, `${rawCase.label}: container`);
      assertEquals(
        actual.isSelected,
        true,
        `${rawCase.label}: latest selected`,
      );
      assertEquals(actual.isDefault, false, `${rawCase.label}: latest default`);
      assertEquals(
        actual.concurrentMarker,
        `latest:${rawCase.label}`,
        `${rawCase.label}: future metadata`,
      );
      assertEquals(
        Object.hasOwn(actual, "icon"),
        rawCase.persistedHasOwn,
        `${rawCase.label}: persisted icon presence`,
      );
      if (rawCase.persistedHasOwn) {
        assertEquals(
          actual.icon,
          rawCase.persistedValue,
          `${rawCase.label}: persisted icon value`,
        );
      } else {
        assert(actual.icon !== null, `${rawCase.label}: did not become null`);
      }
      assertEquals(
        visibilityUpdates,
        1,
        `${rawCase.label}: visibility update count`,
      );
    }

    installOpenWorkspace();
    visibilityUpdates = 0;
    onShow = async () => {
      const data = cloneLiveWorkspaceMap();
      const current = data.get(targetID);
      assert(current, "canonical selection: test workspace disappeared");
      data.set(targetID, {
        ...current,
        icon: "future:latest-before-selection",
        isSelected: true,
        isDefault: false,
        concurrentMarker: "latest:canonical-selection",
      } as ConcurrentWorkspace);
      replaceWorkspaceStore(data, testOrder, originalDefaultID);
      await flushWorkspacePersistence();
      return {
        name: "Canonical selection",
        userContextId: "9",
        icon: "floorp-icon:v1:music",
      };
    };
    await service.manageWorkspaceFromDialog(targetID);
    await flushWorkspacePersistence();
    const selected = cloneLiveWorkspaceMap().get(targetID) as
      | ConcurrentWorkspace
      | undefined;
    assert(selected, "canonical selection: workspace was not retained");
    assertEquals(
      selected.icon,
      "floorp-icon:v1:music",
      "canonical selection overwrites latest raw icon",
    );
    assertEquals(selected.isSelected, true, "canonical latest selected");
    assertEquals(selected.isDefault, false, "canonical latest default");
    assertEquals(
      selected.concurrentMarker,
      "latest:canonical-selection",
      "canonical future metadata",
    );
    assertEquals(visibilityUpdates, 1, "canonical visibility update count");

    installOpenWorkspace();
    visibilityUpdates = 0;
    onShow = async () => {
      const data = cloneLiveWorkspaceMap();
      data.delete(targetID);
      replaceWorkspaceStore(
        data,
        testOrder.filter((id) => id !== targetID),
        originalDefaultID,
      );
      await flushWorkspacePersistence();
      return {
        name: "Must not resurrect",
        userContextId: "0",
        icon: "__floorp_workspace_icon_picker_no_change__",
      };
    };
    const deletedResult = await service.manageWorkspaceFromDialog(targetID);
    await flushWorkspacePersistence();
    assertEquals(deletedResult, undefined, "deleted workspace result");
    assert(
      !cloneLiveWorkspaceMap().has(targetID),
      "deleted workspace was resurrected",
    );
    assert(
      !workspacesDataStore.order.includes(targetID),
      "deleted workspace order entry returned",
    );
    assertEquals(visibilityUpdates, 0, "deleted visibility update count");
  } finally {
    replaceWorkspaceStore(
      new Map(originalData),
      originalOrder,
      originalDefaultID,
    );
    await flushWorkspacePersistence();
    if (hadWorkspacePref) {
      Services.prefs.setStringPref(
        WORKSPACE_DATA_PREF_NAME,
        originalWorkspacePref!,
      );
    } else if (Services.prefs.prefHasUserValue(WORKSPACE_DATA_PREF_NAME)) {
      Services.prefs.clearUserPref(WORKSPACE_DATA_PREF_NAME);
    }
    await flushWorkspacePersistence();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "no-change preserves every raw category",
      fn: testNoChangePreservesEveryRawCategory,
    },
    {
      name: "invalid picker results preserve raw slot",
      fn: testInvalidPickerResultsPreserveRawSlot,
    },
    {
      name: "explicit canonical selection overwrites every category",
      fn: testExplicitCanonicalSelectionOverwritesEveryCategory,
    },
    {
      name: "JSON categories do not collapse to null",
      fn: testJsonCategoriesDoNotCollapseToNull,
    },
    {
      name: "modal form never transports raw stored value",
      fn: testModalFormNeverTransportsRawStoredValue,
    },
    {
      name: "modal result covers cancel no-change invalid and selection",
      fn: testModalResultTransportCoversCancelNoChangeInvalidAndSelection,
    },
    {
      name:
        "service applies modal result to latest workspace without resurrection",
      fn: testServiceUsesLatestWorkspaceWithoutResurrection,
    },
  ];
  await runTests("workspaceIconRawValue.test.ts", tests);
}
