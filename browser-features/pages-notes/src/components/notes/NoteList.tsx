import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, type DragEndEvent } from "@dnd-kit/core";
import { arrayMove, SortableContext, sortableKeyboardCoordinates, verticalListSortingStrategy } from "@dnd-kit/sortable";
import { restrictToVerticalAxis, restrictToFirstScrollableAncestor } from '@dnd-kit/modifiers';
import { NoteItem } from "./NoteItem";
import { useTranslation } from "react-i18next";
import type { Note } from "@/types/note.ts";

interface NoteListProps {
    notes: Note[];
    selectedNote: Note | null;
    onSelectNote: (note: Note) => void;
    onDeleteNote: (id: string) => void;
    onReorderNotes: (notes: Note[]) => void;
    isReorderMode: boolean;
    emptyMessage?: string;
}

export const NoteList = ({
    notes,
    selectedNote,
    onSelectNote,
    onDeleteNote,
    onReorderNotes,
    isReorderMode,
    emptyMessage,
}: NoteListProps) => {
    const { t } = useTranslation();
    const sensors = useSensors(
        useSensor(PointerSensor),
        useSensor(KeyboardSensor, {
            coordinateGetter: sortableKeyboardCoordinates,
        })
    );

    const handleDragEnd = (event: DragEndEvent) => {
        const { active, over } = event;

        if (over && active.id !== over.id) {
            const oldIndex = notes.findIndex(note => note.id === active.id);
            const newIndex = notes.findIndex(note => note.id === over.id);
            const newNotes = arrayMove(notes, oldIndex, newIndex);
            onReorderNotes(newNotes);
        }
    };

    return (
        <div
            className="overflow-y-auto min-h-24 max-h-[40%] shrink-0"
            role="listbox"
            aria-label={t("notes.noteList")}
            onMouseDown={(e) => e.preventDefault()}
        >
            {notes.length === 0 ? (
                <div className="p-4 text-center text-base-content/70">
                    {emptyMessage ?? t("notes.empty")}
                </div>
            ) : (
                <div className="flex flex-col p-2">
                    <DndContext
                        sensors={sensors}
                        collisionDetection={closestCenter}
                        onDragEnd={handleDragEnd}
                        modifiers={[restrictToVerticalAxis, restrictToFirstScrollableAncestor]}
                    >
                        <SortableContext
                            items={notes.map(note => note.id)}
                            strategy={verticalListSortingStrategy}
                        >
                            {notes.map(note => (
                                <NoteItem
                                    key={note.id}
                                    note={note}
                                    isSelected={selectedNote?.id === note.id}
                                    onSelect={isReorderMode ? () => { } : onSelectNote}
                                    onDelete={isReorderMode ? () => { } : onDeleteNote}
                                    isReorderMode={isReorderMode}
                                />
                            ))}
                        </SortableContext>
                    </DndContext>
                </div>
            )}
        </div>
    );
};
