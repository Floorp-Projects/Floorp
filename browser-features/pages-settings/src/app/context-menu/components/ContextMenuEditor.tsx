/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  Fragment,
  type KeyboardEvent,
  memo,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import {
  closestCenter,
  DndContext,
  type DragEndEvent,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
} from "@dnd-kit/core";
import {
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { restrictToVerticalAxis } from "@dnd-kit/modifiers";
import { CSS } from "@dnd-kit/utilities";
import {
  ChevronDown,
  ChevronRight,
  ChevronUp,
  EyeOff,
  GripVertical,
  RefreshCw,
  Search,
  Undo2,
} from "lucide-react";
import type {
  ContextMenuCatalogSnapshot,
  ContextMenuConfig,
  ContextMenuContainerDescriptor,
  ContextMenuItemDescriptor,
} from "#features-chrome/common/context-menu/types.ts";
import { Button } from "@/components/common/button.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import {
  type ContextMenuLevelTarget,
  getContextMenuLevelOverride,
  hasProfileOverride,
  isContextMenuItemHideable,
  isContextMenuItemMovable,
  isContextMenuItemOrderAnchor,
  isProfileIndependent,
  projectContextMenuItemKeysIntoNativeSlots,
} from "../operations.ts";

interface ContextMenuEditorProps {
  catalog: ContextMenuCatalogSnapshot;
  config: ContextMenuConfig;
  disabled?: boolean;
  reloadCatalog(): Promise<void>;
  moveItem(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    overKey: string,
  ): Promise<boolean>;
  setItemVisible(
    target: ContextMenuLevelTarget,
    itemKey: string,
    visible: boolean,
  ): Promise<boolean>;
  setProfileIndependent(
    surfaceKey: string,
    profileKey: string,
    independent: boolean,
  ): Promise<boolean>;
  resetProfile(surfaceKey: string, profileKey: string): Promise<boolean>;
  moveItemBefore(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    beforeKey?: string | null,
  ): Promise<boolean>;
}

interface MenuItemContentProps {
  item: ContextMenuItemDescriptor;
  visible: boolean;
  disabled: boolean;
  movementDisabled: boolean;
  placementActive: boolean;
  childContainerAvailable: boolean;
  canChooseDestination: boolean;
  canMoveUp: boolean;
  canMoveDown: boolean;
  moveButtonRef(node: HTMLButtonElement | null): void;
  onVisibleChange(visible: boolean): void;
  onOpenChild(): void;
  onMoveUp(): void;
  onMoveDown(): void;
  onStartPlacement(): void;
}

interface MenuItemRowProps extends MenuItemContentProps {
  isMovingSource: boolean;
}

interface SortableMenuItemProps extends MenuItemRowProps {
  sortableId: string;
}

type ViewMode = "current" | "all";

interface PlacementOrigin {
  itemKey: string;
  query: string;
  viewMode: ViewMode;
}

interface PlacementDestination {
  id: string;
  beforeKey: string | null;
  position: number;
}

function itemDisplayLabel(
  item: ContextMenuItemDescriptor,
  separatorLabel: string,
): string {
  return item.label.trim() ||
    (item.kind === "separator" ? separatorLabel : item.key);
}

const MenuItemContent = memo(function MenuItemContent({
  item,
  visible,
  disabled,
  movementDisabled,
  placementActive,
  childContainerAvailable,
  canChooseDestination,
  canMoveUp,
  canMoveDown,
  moveButtonRef,
  onVisibleChange,
  onOpenChild,
  onMoveUp,
  onMoveDown,
  onStartPlacement,
}: MenuItemContentProps) {
  const { t } = useTranslation();
  const movable = isContextMenuItemMovable(item);
  const hideable = isContextMenuItemHideable(item);
  const label = itemDisplayLabel(item, t("contextMenu.separator"));

  return (
    <>
      <div className="min-w-0 flex-1">
        <div className="flex min-w-0 flex-wrap items-center gap-2">
          <span className="truncate font-medium">{label}</span>
          <span className="badge badge-sm badge-ghost">
            {t(`contextMenu.kind.${item.kind}`)}
          </span>
          <span className="badge badge-sm badge-outline">
            {t(`contextMenu.source.${item.source}`)}
          </span>
          {item.nativeHidden && (
            <span className="badge badge-sm badge-warning gap-1">
              <EyeOff className="size-3" />
              {t("contextMenu.nativeHidden")}
            </span>
          )}
          {!movable && (
            <span className="badge badge-sm badge-neutral">
              {t("contextMenu.notMovable")}
            </span>
          )}
        </div>
        <p className="mt-1 truncate font-mono text-xs text-base-content/50">
          {item.key}
        </p>
      </div>

      {movable && (
        <div className="flex shrink-0 items-center gap-1">
          <div
            className="join"
            role="group"
            aria-label={t("contextMenu.moveItem", { label })}
          >
            <button
              type="button"
              className="btn btn-ghost btn-sm btn-square join-item"
              disabled={disabled || movementDisabled || !canMoveUp}
              onClick={onMoveUp}
              aria-label={t("contextMenu.moveItemUp", { label })}
              title={t("contextMenu.moveUp")}
            >
              <ChevronUp className="size-4" />
            </button>
            <button
              type="button"
              className="btn btn-ghost btn-sm btn-square join-item"
              disabled={disabled || movementDisabled || !canMoveDown}
              onClick={onMoveDown}
              aria-label={t("contextMenu.moveItemDown", { label })}
              title={t("contextMenu.moveDown")}
            >
              <ChevronDown className="size-4" />
            </button>
          </div>
          {!placementActive && (
            <button
              ref={moveButtonRef}
              type="button"
              className="btn btn-ghost btn-sm shrink-0"
              disabled={disabled || !canChooseDestination}
              onClick={onStartPlacement}
              aria-label={t("contextMenu.moveDestinationFor", { label })}
            >
              {t("contextMenu.moveToDestination")}
            </button>
          )}
        </div>
      )}

      {childContainerAvailable && (
        <button
          type="button"
          className="btn btn-ghost btn-sm shrink-0"
          disabled={disabled || placementActive}
          onClick={onOpenChild}
          aria-label={t("contextMenu.openSubmenu", { label })}
        >
          {t("contextMenu.editSubmenu")}
          <ChevronRight className="size-4" />
        </button>
      )}

      {hideable
        ? (
          <Switch
            size="sm"
            checked={visible}
            disabled={disabled || placementActive}
            onChange={(event) => onVisibleChange(event.currentTarget.checked)}
            aria-label={t("contextMenu.itemVisibility", { label })}
          />
        )
        : (
          <span
            className="badge badge-sm badge-ghost shrink-0"
            aria-label={t("contextMenu.itemVisibilityUnavailable", { label })}
          >
            {item.kind === "separator"
              ? t("contextMenu.separatorVisibilityAutomatic")
              : t("contextMenu.visibilityFixed")}
          </span>
        )}
    </>
  );
});

function rowClassName(
  visible: boolean,
  isDragging: boolean,
  isMovingSource: boolean,
): string {
  return `flex min-w-0 items-center gap-3 border-b border-base-300/30 bg-base-100 px-3 py-3 last:border-b-0 ${
    isDragging ? "z-20 opacity-60 shadow-lg" : ""
  } ${visible ? "" : "opacity-60"} ${
    isMovingSource ? "relative z-10 ring-2 ring-inset ring-primary/60" : ""
  }`;
}

function SortableMenuItem({
  sortableId,
  item,
  visible,
  disabled,
  isMovingSource,
  ...contentProps
}: SortableMenuItemProps) {
  const { t } = useTranslation();
  const movable = isContextMenuItemMovable(item);
  const orderAnchor = isContextMenuItemOrderAnchor(item);
  const label = itemDisplayLabel(item, t("contextMenu.separator"));
  const {
    attributes,
    listeners,
    setActivatorNodeRef,
    setNodeRef,
    transform,
    transition,
    isDragging,
  } = useSortable({
    id: sortableId,
    disabled: {
      draggable: disabled || !movable,
      droppable: disabled || !orderAnchor,
    },
  });

  return (
    <div
      ref={setNodeRef}
      style={{
        transform: CSS.Transform.toString(transform),
        transition,
      }}
      className={rowClassName(visible, isDragging, isMovingSource)}
    >
      <button
        ref={setActivatorNodeRef}
        type="button"
        className="btn btn-ghost btn-sm btn-square shrink-0 cursor-grab touch-none disabled:cursor-not-allowed"
        disabled={disabled || !movable}
        aria-label={movable
          ? t("contextMenu.dragItem", { label })
          : t("contextMenu.cannotDragItem", { label })}
        {...attributes}
        {...listeners}
      >
        <GripVertical className="size-4" />
      </button>
      <MenuItemContent
        item={item}
        visible={visible}
        disabled={disabled}
        {...contentProps}
      />
    </div>
  );
}

function PlainMenuItem({
  item,
  visible,
  disabled,
  isMovingSource,
  ...contentProps
}: MenuItemRowProps) {
  return (
    <div className={rowClassName(visible, false, isMovingSource)}>
      <span className="size-8 shrink-0" aria-hidden="true" />
      <MenuItemContent
        item={item}
        visible={visible}
        disabled={disabled}
        {...contentProps}
      />
    </div>
  );
}

function PlacementGap({
  label,
  disabled,
  tabIndex,
  buttonRef,
  onFocus,
  onKeyDown,
  onClick,
}: {
  label: string;
  disabled: boolean;
  tabIndex: number;
  buttonRef(node: HTMLButtonElement | null): void;
  onFocus(): void;
  onKeyDown(event: KeyboardEvent<HTMLButtonElement>): void;
  onClick(): void;
}) {
  return (
    <button
      ref={buttonRef}
      type="button"
      className="group flex min-h-11 w-full items-center gap-3 bg-primary/5 px-3 text-xs font-medium text-primary transition-colors hover:bg-primary/15 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-inset focus-visible:ring-primary disabled:opacity-50"
      disabled={disabled}
      tabIndex={tabIndex}
      onClick={onClick}
      onFocus={onFocus}
      onKeyDown={onKeyDown}
      aria-label={label}
    >
      <span className="h-px flex-1 bg-primary/35 group-hover:bg-primary" />
      <span>{label}</span>
      <span className="h-px flex-1 bg-primary/35 group-hover:bg-primary" />
    </button>
  );
}

function findContainer(
  containers: readonly ContextMenuContainerDescriptor[],
  selectedKey: string,
): ContextMenuContainerDescriptor | undefined {
  return containers.find((container) => container.key === selectedKey) ??
    containers.find((container) => container.key === "root") ??
    containers[0];
}

export function ContextMenuEditor({
  catalog,
  config,
  disabled = false,
  reloadCatalog,
  moveItem,
  moveItemBefore,
  setItemVisible,
  setProfileIndependent,
  resetProfile,
}: ContextMenuEditorProps) {
  const { t } = useTranslation();
  const [selectedSurfaceKey, setSelectedSurfaceKey] = useState("");
  const [selectedProfileKey, setSelectedProfileKey] = useState("");
  const [selectedContainerKey, setSelectedContainerKey] = useState("");
  const [query, setQuery] = useState("");
  const [viewMode, setViewMode] = useState<ViewMode>("current");
  const [placementOrigin, setPlacementOrigin] = useState<
    PlacementOrigin | null
  >(
    null,
  );
  const [placementPending, setPlacementPending] = useState(false);
  const [focusedDestinationId, setFocusedDestinationId] = useState("");
  const [moveStatus, setMoveStatus] = useState("");
  const moveButtonRefs = useRef(new Map<string, HTMLButtonElement>());
  const destinationRefs = useRef(new Map<string, HTMLButtonElement>());
  const placementSessionRef = useRef(0);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 4 } }),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    }),
  );

  const selectedSurface =
    catalog.surfaces.find((surface) => surface.key === selectedSurfaceKey) ??
      catalog.surfaces[0];
  const selectedProfile =
    selectedSurface?.profiles.find((profile) =>
      profile.key === selectedProfileKey
    ) ?? selectedSurface?.profiles[0];
  const selectedContainer = findContainer(
    selectedProfile?.containers ?? [],
    selectedContainerKey,
  );

  const target = useMemo<ContextMenuLevelTarget | null>(() => {
    if (!selectedSurface || !selectedProfile || !selectedContainer) {
      return null;
    }
    return {
      surfaceKey: selectedSurface.key,
      profileKey: selectedProfile.key,
      containerKey: selectedContainer.key,
    };
  }, [selectedContainer, selectedProfile, selectedSurface]);

  const orderedEntries = useMemo(() => {
    if (!target || !selectedContainer) return [];
    const itemsByKey = new Map<string, ContextMenuItemDescriptor[]>();
    const instanceIds = new Map<ContextMenuItemDescriptor, string>();
    selectedContainer.items.forEach((item, index) => {
      const queue = itemsByKey.get(item.key) ?? [];
      queue.push(item);
      itemsByKey.set(item.key, queue);
      instanceIds.set(
        item,
        item.catalogInstanceId ?? `${index}:${item.key}`,
      );
    });
    const keys = projectContextMenuItemKeysIntoNativeSlots(
      selectedContainer.items,
      getContextMenuLevelOverride(config, target),
    );
    return keys.flatMap((key) => {
      const item = itemsByKey.get(key)?.shift();
      return item
        ? [{ item, sortableId: instanceIds.get(item) ?? item.key }]
        : [];
    });
  }, [config, selectedContainer, target]);

  const displayedEntries = useMemo(
    () =>
      viewMode === "all"
        ? orderedEntries
        : orderedEntries.filter(({ item }) => !item.nativeHidden),
    [orderedEntries, viewMode],
  );
  const normalizedQuery = query.trim().toLocaleLowerCase();
  const filteredEntries = normalizedQuery
    ? displayedEntries.filter(({ item }) =>
      `${item.label} ${item.key}`.toLocaleLowerCase().includes(
        normalizedQuery,
      )
    )
    : displayedEntries;
  const moveTargetsByKey = useMemo(() => {
    const anchorKeys = displayedEntries.flatMap(({ item }) =>
      isContextMenuItemOrderAnchor(item) ? [item.key] : []
    );
    return new Map(
      anchorKeys.map((key, index) => [
        key,
        {
          up: anchorKeys[index - 1],
          down: anchorKeys[index + 1],
        },
      ]),
    );
  }, [displayedEntries]);
  const allAnchorKeys = useMemo(
    () =>
      orderedEntries.flatMap(({ item }) =>
        isContextMenuItemOrderAnchor(item) ? [item.key] : []
      ),
    [orderedEntries],
  );
  const movingItem = placementOrigin
    ? orderedEntries.find(({ item }) => item.key === placementOrigin.itemKey)
      ?.item
    : undefined;
  const placementDestinations = useMemo<PlacementDestination[]>(() => {
    if (!placementOrigin || !movingItem) return [];
    const activeIndex = allAnchorKeys.indexOf(placementOrigin.itemKey);
    if (activeIndex < 0) return [];
    const currentBeforeKey = allAnchorKeys[activeIndex + 1] ?? null;
    const beforeDestinations = allAnchorKeys.flatMap((beforeKey, position) =>
      beforeKey === placementOrigin.itemKey || beforeKey === currentBeforeKey
        ? []
        : [{ id: `before:${beforeKey}`, beforeKey, position }]
    );
    return currentBeforeKey === null ? beforeDestinations : [
      ...beforeDestinations,
      { id: "end", beforeKey: null, position: allAnchorKeys.length },
    ];
  }, [allAnchorKeys, movingItem, placementOrigin]);
  const destinationsByBeforeKey = useMemo(
    () =>
      new Map(
        placementDestinations.map((destination) => [
          destination.beforeKey,
          destination,
        ]),
      ),
    [placementDestinations],
  );
  const hiddenKeys = useMemo(
    () =>
      new Set(
        target ? getContextMenuLevelOverride(config, target).hidden ?? [] : [],
      ),
    [config, target],
  );
  const containerKeySet = useMemo(
    () =>
      new Set(
        selectedProfile?.containers.map((container) => container.key) ?? [],
      ),
    [selectedProfile],
  );
  const independent = selectedSurface && selectedProfile
    ? isProfileIndependent(
      config,
      selectedSurface.key,
      selectedProfile.key,
    )
    : false;
  const profileConfigured = selectedSurface && selectedProfile
    ? hasProfileOverride(config, selectedSurface.key, selectedProfile.key)
    : false;

  const focusMoveButton = useCallback((itemKey: string) => {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => moveButtonRefs.current.get(itemKey)?.focus());
    });
  }, []);

  const restorePlacement = useCallback((
    origin: PlacementOrigin,
    shouldRestoreFocus: boolean,
  ) => {
    setPlacementOrigin(null);
    setPlacementPending(false);
    setFocusedDestinationId("");
    setViewMode(origin.viewMode);
    setQuery(origin.query);
    if (shouldRestoreFocus) focusMoveButton(origin.itemKey);
  }, [focusMoveButton]);

  const cancelPlacement = useCallback((
    restoreFocus = true,
    force = false,
  ) => {
    if (!placementOrigin || (placementPending && !force)) return;
    placementSessionRef.current += 1;
    setMoveStatus(t("contextMenu.cancelMove"));
    restorePlacement(placementOrigin, restoreFocus);
  }, [placementOrigin, placementPending, restorePlacement, t]);

  useEffect(() => {
    if (!placementOrigin) return;
    const handleEscape = (event: globalThis.KeyboardEvent) => {
      if (event.key !== "Escape") return;
      event.preventDefault();
      cancelPlacement();
    };
    globalThis.addEventListener("keydown", handleEscape);
    return () => globalThis.removeEventListener("keydown", handleEscape);
  }, [cancelPlacement, placementOrigin]);

  useEffect(() => {
    if (
      !placementOrigin || placementPending ||
      placementDestinations.length === 0
    ) return;
    const activeIndex = allAnchorKeys.indexOf(placementOrigin.itemKey);
    const destination = placementDestinations.reduce((closest, candidate) =>
      Math.abs(candidate.position - activeIndex) <
          Math.abs(closest.position - activeIndex)
        ? candidate
        : closest
    );
    setFocusedDestinationId(destination.id);
    const frame = requestAnimationFrame(() =>
      destinationRefs.current.get(destination.id)?.focus()
    );
    return () => cancelAnimationFrame(frame);
  }, [
    allAnchorKeys,
    placementDestinations,
    placementOrigin,
    placementPending,
  ]);

  useEffect(() => {
    if (
      placementOrigin &&
      (!movingItem || !isContextMenuItemMovable(movingItem))
    ) {
      cancelPlacement(false, true);
    }
  }, [cancelPlacement, movingItem, placementOrigin]);

  const handleDragEnd = (event: DragEndEvent) => {
    if (disabled || !target || !selectedContainer || !event.over) return;
    const activeEntry = orderedEntries.find((entry) =>
      entry.sortableId === String(event.active.id)
    );
    const overEntry = orderedEntries.find((entry) =>
      entry.sortableId === String(event.over?.id)
    );
    if (!activeEntry || !overEntry) return;
    const activeKey = activeEntry.item.key;
    const overKey = overEntry.item.key;
    if (activeKey === overKey) return;
    void moveItem(
      target,
      selectedContainer.items,
      activeKey,
      overKey,
    );
  };

  const handleButtonMove = (
    itemKey: string,
    direction: "up" | "down",
  ) => {
    if (disabled || !target || !selectedContainer) return;
    const overKey = moveTargetsByKey.get(itemKey)?.[direction];
    if (!overKey) return;
    void moveItem(target, selectedContainer.items, itemKey, overKey);
  };

  const startPlacement = (itemKey: string) => {
    if (
      disabled || placementOrigin || allAnchorKeys.length < 2 ||
      !orderedEntries.some(({ item }) =>
        item.key === itemKey && isContextMenuItemMovable(item)
      )
    ) return;
    placementSessionRef.current += 1;
    setPlacementOrigin({ itemKey, query, viewMode });
    setPlacementPending(false);
    setFocusedDestinationId("");
    setMoveStatus("");
    setViewMode("all");
    setQuery("");
  };

  const placeItem = async (destination: PlacementDestination) => {
    if (
      disabled || placementPending || !placementOrigin || !movingItem ||
      !target || !selectedContainer ||
      !placementDestinations.some(({ id }) => id === destination.id)
    ) return;
    const session = placementSessionRef.current;
    const origin = placementOrigin;
    const sourceLabel = itemDisplayLabel(
      movingItem,
      t("contextMenu.separator"),
    );
    const targetItem = destination.beforeKey === null
      ? undefined
      : orderedEntries.find(({ item }) => item.key === destination.beforeKey)
        ?.item;
    setPlacementPending(true);
    const saved = await moveItemBefore(
      target,
      selectedContainer.items,
      origin.itemKey,
      destination.beforeKey,
    );
    if (placementSessionRef.current !== session) return;
    if (!saved) {
      setPlacementPending(false);
      return;
    }
    placementSessionRef.current += 1;
    setMoveStatus(
      targetItem
        ? t("contextMenu.movedBefore", {
          label: sourceLabel,
          target: itemDisplayLabel(targetItem, t("contextMenu.separator")),
        })
        : t("contextMenu.movedToEnd", { label: sourceLabel }),
    );
    restorePlacement(origin, true);
  };

  const handleDestinationKeyDown = (
    event: KeyboardEvent<HTMLButtonElement>,
    destinationId: string,
  ) => {
    const currentIndex = placementDestinations.findIndex(({ id }) =>
      id === destinationId
    );
    if (currentIndex < 0) return;
    let nextIndex: number | undefined;
    if (event.key === "ArrowDown" || event.key === "ArrowRight") {
      nextIndex = (currentIndex + 1) % placementDestinations.length;
    } else if (event.key === "ArrowUp" || event.key === "ArrowLeft") {
      nextIndex = (currentIndex - 1 + placementDestinations.length) %
        placementDestinations.length;
    } else if (event.key === "Home") {
      nextIndex = 0;
    } else if (event.key === "End") {
      nextIndex = placementDestinations.length - 1;
    }
    if (nextIndex === undefined) return;
    event.preventDefault();
    const nextDestination = placementDestinations[nextIndex];
    setFocusedDestinationId(nextDestination.id);
    destinationRefs.current.get(nextDestination.id)?.focus();
  };

  const sortableView = viewMode === "current" && !normalizedQuery &&
    !placementOrigin;
  const renderMenuItem = (
    { item, sortableId }: (typeof filteredEntries)[number],
  ) => {
    const moveTargets = moveTargetsByKey.get(item.key);
    const commonProps: MenuItemRowProps = {
      item,
      visible: !isContextMenuItemHideable(item) || !hiddenKeys.has(item.key),
      disabled,
      movementDisabled: Boolean(placementOrigin || normalizedQuery),
      placementActive: placementOrigin !== null,
      isMovingSource: placementOrigin?.itemKey === item.key,
      childContainerAvailable: item.childContainerKey !== undefined &&
        containerKeySet.has(item.childContainerKey),
      canChooseDestination: allAnchorKeys.length > 1,
      canMoveUp: moveTargets?.up !== undefined,
      canMoveDown: moveTargets?.down !== undefined,
      moveButtonRef: (node) => {
        if (node) moveButtonRefs.current.set(item.key, node);
        else moveButtonRefs.current.delete(item.key);
      },
      onVisibleChange: (visible) =>
        void setItemVisible(target!, item.key, visible),
      onOpenChild: () => {
        if (item.childContainerKey) {
          setSelectedContainerKey(item.childContainerKey);
          setQuery("");
          setMoveStatus("");
        }
      },
      onMoveUp: () => handleButtonMove(item.key, "up"),
      onMoveDown: () => handleButtonMove(item.key, "down"),
      onStartPlacement: () => startPlacement(item.key),
    };
    return sortableView
      ? (
        <SortableMenuItem
          key={sortableId}
          sortableId={sortableId}
          {...commonProps}
        />
      )
      : <PlainMenuItem key={sortableId} {...commonProps} />;
  };

  const renderPlacementGap = (destination: PlacementDestination) => {
    const destinationItem = destination.beforeKey === null
      ? undefined
      : orderedEntries.find(({ item }) => item.key === destination.beforeKey)
        ?.item;
    const label = destinationItem
      ? t("contextMenu.moveBefore", {
        label: itemDisplayLabel(destinationItem, t("contextMenu.separator")),
      })
      : t("contextMenu.moveToEnd");
    return (
      <PlacementGap
        label={label}
        disabled={disabled || placementPending}
        tabIndex={focusedDestinationId === destination.id ? 0 : -1}
        buttonRef={(node) => {
          if (node) destinationRefs.current.set(destination.id, node);
          else destinationRefs.current.delete(destination.id);
        }}
        onFocus={() => setFocusedDestinationId(destination.id)}
        onKeyDown={(event) => handleDestinationKeyDown(event, destination.id)}
        onClick={() => void placeItem(destination)}
      />
    );
  };
  const itemList = (
    <div
      id="context-menu-items-list"
      className="overflow-hidden rounded-lg border border-base-300/30"
    >
      {filteredEntries.map((entry) => {
        const destination = destinationsByBeforeKey.get(entry.item.key);
        return (
          <Fragment key={entry.sortableId}>
            {destination && renderPlacementGap(destination)}
            {renderMenuItem(entry)}
          </Fragment>
        );
      })}
      {destinationsByBeforeKey.has(null) &&
        renderPlacementGap(destinationsByBeforeKey.get(null)!)}
    </div>
  );

  if (!selectedSurface) return null;

  return (
    <>
      <Card>
        <CardHeader className="flex flex-row items-start justify-between gap-4">
          <div>
            <CardTitle>{t("contextMenu.menuSelection")}</CardTitle>
            <CardDescription>
              {t("contextMenu.menuSelectionDescription")}
            </CardDescription>
          </div>
          <Button
            type="button"
            size="sm"
            variant="ghost"
            className="shrink-0"
            onClick={() => {
              cancelPlacement(false, true);
              setMoveStatus("");
              void reloadCatalog();
            }}
          >
            <RefreshCw className="mr-2 size-4" />
            {t("contextMenu.refreshCatalog")}
          </Button>
        </CardHeader>
        <CardContent className="space-y-5">
          <label className="form-control w-full">
            <span className="label-text mb-2 text-sm font-medium">
              {t("contextMenu.surface")}
            </span>
            <select
              className="select select-bordered w-full"
              value={selectedSurface.key}
              onChange={(event) => {
                cancelPlacement(false, true);
                setSelectedSurfaceKey(event.currentTarget.value);
                setSelectedProfileKey("");
                setSelectedContainerKey("");
                setQuery("");
                setMoveStatus("");
              }}
            >
              {catalog.surfaces.map((surface) => (
                <option key={surface.key} value={surface.key}>
                  {surface.label || surface.key}
                </option>
              ))}
            </select>
          </label>

          {selectedSurface.profiles.length === 0
            ? (
              <p className="rounded-lg bg-base-100 p-4 text-sm text-base-content/70">
                {t("contextMenu.noProfiles")}
              </p>
            )
            : (
              <div>
                <p className="mb-2 text-sm font-medium">
                  {t("contextMenu.profile")}
                </p>
                <div
                  className="flex flex-wrap gap-2"
                  role="tablist"
                  aria-label={t("contextMenu.profile")}
                >
                  {selectedSurface.profiles.map((profile) => (
                    <button
                      key={profile.key}
                      type="button"
                      role="tab"
                      aria-selected={profile.key === selectedProfile?.key}
                      className={`btn btn-sm ${
                        profile.key === selectedProfile?.key
                          ? "btn-primary"
                          : "btn-ghost"
                      }`}
                      onClick={() => {
                        cancelPlacement(false, true);
                        setSelectedProfileKey(profile.key);
                        setSelectedContainerKey("");
                        setQuery("");
                        setMoveStatus("");
                      }}
                    >
                      {profile.label || profile.key}
                    </button>
                  ))}
                </div>
              </div>
            )}

          {selectedProfile && (
            <div className="flex flex-col gap-3 rounded-lg bg-base-100 p-4 sm:flex-row sm:items-center sm:justify-between">
              <div>
                <p className="font-medium">
                  {t("contextMenu.independentProfile")}
                </p>
                <p className="mt-1 text-sm text-base-content/60">
                  {independent
                    ? t("contextMenu.independentProfileOnDescription")
                    : t("contextMenu.independentProfileOffDescription")}
                </p>
              </div>
              <div className="flex shrink-0 items-center gap-2">
                <Button
                  type="button"
                  size="sm"
                  variant="ghost"
                  disabled={disabled || placementOrigin !== null ||
                    !profileConfigured}
                  onClick={() =>
                    void resetProfile(
                      selectedSurface.key,
                      selectedProfile.key,
                    )}
                >
                  <Undo2 className="mr-2 size-4" />
                  {t("contextMenu.resetProfile")}
                </Button>
                <Switch
                  checked={independent}
                  disabled={disabled || placementOrigin !== null}
                  onChange={(event) =>
                    void setProfileIndependent(
                      selectedSurface.key,
                      selectedProfile.key,
                      event.currentTarget.checked,
                    )}
                  aria-label={t("contextMenu.independentProfile")}
                />
              </div>
            </div>
          )}
        </CardContent>
      </Card>

      {selectedProfile && selectedContainer && target && (
        <Card>
          <CardHeader>
            <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
              <div>
                <CardTitle>{t("contextMenu.items")}</CardTitle>
                <CardDescription>
                  {independent
                    ? t("contextMenu.editingIndependent")
                    : t("contextMenu.editingShared")}
                </CardDescription>
              </div>
              <label className="form-control w-full sm:w-64">
                <span className="sr-only">{t("contextMenu.container")}</span>
                <select
                  className="select select-bordered select-sm w-full"
                  value={selectedContainer.key}
                  onChange={(event) => {
                    cancelPlacement(false, true);
                    setSelectedContainerKey(event.currentTarget.value);
                    setQuery("");
                    setMoveStatus("");
                  }}
                >
                  {selectedProfile.containers.map((container) => (
                    <option key={container.key} value={container.key}>
                      {container.label || container.key}
                    </option>
                  ))}
                </select>
              </label>
            </div>
          </CardHeader>
          <CardContent className="space-y-4">
            {!selectedContainer.complete && (
              <p className="rounded-lg border border-warning/40 bg-warning/10 p-3 text-sm">
                {t("contextMenu.incompleteContainer")}
              </p>
            )}

            <p className="rounded-lg bg-base-200/60 p-3 text-sm text-base-content/70">
              {t("contextMenu.capabilityHelp")}
            </p>

            <div className="space-y-2">
              <p className="text-sm font-medium">
                {t("contextMenu.viewMode")}
              </p>
              <div
                className="join"
                role="tablist"
                aria-label={t("contextMenu.viewMode")}
              >
                {(["current", "all"] as const).map((mode) => (
                  <button
                    key={mode}
                    type="button"
                    role="tab"
                    aria-selected={viewMode === mode}
                    aria-controls="context-menu-items-list"
                    disabled={placementOrigin !== null}
                    className={`btn btn-sm join-item ${
                      viewMode === mode ? "btn-primary" : "btn-ghost"
                    }`}
                    onClick={() => {
                      setViewMode(mode);
                      setMoveStatus("");
                    }}
                  >
                    {t(
                      mode === "current"
                        ? "contextMenu.viewCurrent"
                        : "contextMenu.viewAll",
                    )}
                  </button>
                ))}
              </div>
              <p className="text-sm text-base-content/60">
                {t(
                  viewMode === "current"
                    ? "contextMenu.viewCurrentDescription"
                    : "contextMenu.viewAllDescription",
                )}
              </p>
            </div>

            {viewMode === "all" && !placementOrigin && (
              <p className="rounded-lg border border-info/30 bg-info/10 p-3 text-sm text-base-content/70">
                {t("contextMenu.allModeDragHelp")}
              </p>
            )}

            {placementOrigin && movingItem && (
              <div className="flex flex-col gap-3 rounded-lg border border-primary/40 bg-primary/10 p-3 sm:flex-row sm:items-center sm:justify-between">
                <p className="font-medium">
                  {t("contextMenu.placementInstruction", {
                    label: itemDisplayLabel(
                      movingItem,
                      t("contextMenu.separator"),
                    ),
                  })}
                </p>
                <Button
                  type="button"
                  size="sm"
                  variant="ghost"
                  disabled={placementPending}
                  onClick={() => cancelPlacement()}
                >
                  {t("contextMenu.cancelMove")}
                </Button>
              </div>
            )}

            <div
              className="min-h-5 text-sm text-success"
              role="status"
              aria-live="polite"
              aria-atomic="true"
            >
              {moveStatus}
            </div>

            <label className="input input-bordered flex w-full items-center gap-2">
              <Search className="size-4 text-base-content/50" />
              <input
                type="search"
                className="grow"
                value={query}
                disabled={placementOrigin !== null}
                onChange={(event) => {
                  setQuery(event.currentTarget.value);
                  setMoveStatus("");
                }}
                placeholder={t("contextMenu.searchItems")}
              />
            </label>

            {selectedContainer.items.length === 0
              ? (
                <p className="rounded-lg bg-base-100 p-6 text-center text-sm text-base-content/60">
                  {t("contextMenu.noItems")}
                </p>
              )
              : displayedEntries.length === 0 && viewMode === "current"
              ? (
                <p className="rounded-lg bg-base-100 p-6 text-center text-sm text-base-content/60">
                  {t("contextMenu.noCurrentItems")}
                </p>
              )
              : filteredEntries.length === 0
              ? (
                <p className="rounded-lg bg-base-100 p-6 text-center text-sm text-base-content/60">
                  {t("contextMenu.noMatchingItems")}
                </p>
              )
              : (
                sortableView
                  ? (
                    <DndContext
                      sensors={sensors}
                      collisionDetection={closestCenter}
                      modifiers={[restrictToVerticalAxis]}
                      onDragEnd={handleDragEnd}
                    >
                      <SortableContext
                        items={filteredEntries.map((entry) => entry.sortableId)}
                        strategy={verticalListSortingStrategy}
                      >
                        {itemList}
                      </SortableContext>
                    </DndContext>
                  )
                  : itemList
              )}
          </CardContent>
        </Card>
      )}
    </>
  );
}
