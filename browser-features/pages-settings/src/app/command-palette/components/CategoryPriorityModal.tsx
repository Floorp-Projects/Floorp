import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
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
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { restrictToParentElement } from "@dnd-kit/modifiers";
import { CSS } from "@dnd-kit/utilities";
import { GripVertical } from "lucide-react";
import { Button } from "@/components/common/button.tsx";
import { DEFAULT_CATEGORY_PRIORITY } from "../dataManager.ts";
import type { CommandPaletteFormData } from "@/types/pref.ts";

interface CategoryPriorityModalProps {
  isOpen: boolean;
  onClose: () => void;
}

interface SortableCategoryRowProps {
  category: string;
  ordinal: number;
  displayName: string;
}

const SortableCategoryRow = ({
  category,
  ordinal,
  displayName,
}: SortableCategoryRowProps) => {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } =
    useSortable({ id: category });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className={
        "flex items-center gap-3 px-3 py-2 bg-base-100 rounded-lg" +
        (isDragging ? " opacity-60 shadow-lg ring-1 ring-base-300" : "")
      }
    >
      <span className="badge badge-sm badge-ghost shrink-0 w-6 justify-center font-mono">
        {ordinal}
      </span>
      <button
        type="button"
        className="cursor-grab active:cursor-grabbing text-base-content/50 hover:text-base-content touch-none focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary focus-visible:ring-offset-1"
        aria-label={`Drag to reorder ${displayName}`}
        {...attributes}
        {...listeners}
      >
        <GripVertical className="size-4" />
      </button>
      <span className="flex-1 truncate">{displayName}</span>
      <span className="text-xs text-base-content/40 font-mono">{category}</span>
    </div>
  );
};

export function CategoryPriorityModal({
  isOpen,
  onClose,
}: CategoryPriorityModalProps) {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();
  const dialogRef = useRef<HTMLDialogElement>(null);
  const [order, setOrder] = useState<string[]>([...DEFAULT_CATEGORY_PRIORITY]);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 8 } }),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    }),
  );

  // Load the current priority from the form whenever the modal opens.
  useEffect(() => {
    if (!isOpen) return;
    const current = getValues("categoryPriority");
    if (Array.isArray(current) && current.length > 0) {
      setOrder([...current]);
    } else {
      setOrder([...DEFAULT_CATEGORY_PRIORITY]);
    }
  }, [isOpen, getValues]);

  // Drive the native <dialog> element open/close.
  useEffect(() => {
    if (isOpen) {
      dialogRef.current?.showModal();
    } else {
      dialogRef.current?.close();
    }
  }, [isOpen]);

  const handleDragEnd = (event: DragEndEvent) => {
    const { active, over } = event;
    if (!over || active.id === over.id) return;
    const oldIndex = order.indexOf(String(active.id));
    const newIndex = order.indexOf(String(over.id));
    if (oldIndex === -1 || newIndex === -1) return;
    const next = arrayMove(order, oldIndex, newIndex);
    setValue("categoryPriority", next, { shouldValidate: true });
    setOrder(next);
  };

  const handleReset = () => {
    const next = [...DEFAULT_CATEGORY_PRIORITY];
    setOrder(next);
    setValue("categoryPriority", next, { shouldValidate: true });
  };

  const getDisplayName = (category: string): string => {
    const key = `commandPalette.categoryNames.${category}`;
    const translated = t(key);
    // i18next returns the key itself when the entry is missing.
    return translated === key ? category : translated;
  };

  return (
    <dialog
      ref={dialogRef}
      className="modal"
      onClose={onClose}
      aria-labelledby="category-priority-modal-title"
    >
      <div className="modal-box max-w-2xl">
        <h3 id="category-priority-modal-title" className="font-bold text-lg">
          {t("commandPalette.priorityModalTitle")}
        </h3>
        <p className="py-1 text-sm text-base-content/70">
          {t("commandPalette.priorityModalDescription")}
        </p>
        <div className="py-4 max-h-[60vh] overflow-y-auto space-y-2">
          <DndContext
            sensors={sensors}
            collisionDetection={closestCenter}
            onDragEnd={handleDragEnd}
            modifiers={[restrictToParentElement]}
          >
            <SortableContext
              items={order}
              strategy={verticalListSortingStrategy}
            >
              {order.map((category, index) => (
                <SortableCategoryRow
                  key={category}
                  category={category}
                  ordinal={index + 1}
                  displayName={getDisplayName(category)}
                />
              ))}
            </SortableContext>
          </DndContext>
        </div>
        <div className="modal-action">
          <Button onClick={handleReset} variant="ghost">
            {t("commandPalette.priorityModalReset")}
          </Button>
          <Button onClick={onClose} variant="primary">
            {t("commandPalette.priorityModalClose")}
          </Button>
        </div>
      </div>
      <form method="dialog" className="modal-backdrop">
        <button type="button" onClick={onClose} />
      </form>
    </dialog>
  );
}
