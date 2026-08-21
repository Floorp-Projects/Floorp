import { memo, useMemo } from "react";
import { CSS } from "@dnd-kit/utilities";
import { useSortable } from "@dnd-kit/sortable";
import { MoveVertical, X } from "lucide-react";
import { useTranslation } from "react-i18next";
import type { Note } from "@/types/note.ts";

interface NoteItemProps {
    note: Note;
    isSelected: boolean;
    onSelect: (note: Note) => void;
    onDelete: (id: string) => void;
    isReorderMode: boolean;
}

import { extractPlainText } from "@/lib/extractText.ts";

function extractContent(content: string, emptyLabel: string): string {
    if (!content || content.length === 0) {
        return emptyLabel;
    }
    return extractPlainText(content) || emptyLabel;
}

export const NoteItem = memo(function NoteItem({ note, isSelected, onSelect, onDelete, isReorderMode }: NoteItemProps) {
    const { t, i18n } = useTranslation();
    const {
        attributes,
        listeners,
        setNodeRef,
        transform,
        transition
    } = useSortable({ id: note.id });

    const style = {
        transform: CSS.Transform.toString(transform),
        transition,
    };

    const formattedDate = useMemo(() => {
        // i18n.language may contain non-BCP47 tags like "ja-JP-mac" (Mozilla-specific).
        // Strip the trailing variant to get a valid Intl locale.
        const locale = i18n.language.replace(/-mac$/, "");
        try {
            return new Intl.DateTimeFormat(locale, {
                year: 'numeric',
                month: '2-digit',
                day: '2-digit',
                hour: '2-digit',
                minute: '2-digit'
            }).format(new Date(note.updatedAt));
        } catch {
            return new Date(note.updatedAt).toLocaleString();
        }
    }, [note.updatedAt, i18n.language]);

    const contentPreview = useMemo(
        () => extractContent(note.content, t("notes.emptyContent")),
        [note.content, t],
    );

    return (
        <div
            ref={setNodeRef}
            style={style}
            className="mb-1"
            {...(isReorderMode ? attributes : {})}
        >
            <button
                type="button"
                data-testid="notes-row"
                role="option"
                aria-selected={isSelected}
                className={`w-full flex flex-col px-2 py-1.5 rounded-lg border transition-colors text-sm ${isReorderMode
                    ? 'cursor-grab active:cursor-grabbing border-secondary/50'
                    : ''
                    } ${isSelected
                        ? 'bg-primary/10 border-primary'
                        : 'hover:bg-base-200 border-base-content/5'
                    }`}
                onClick={() => onSelect(note)}
                onContextMenu={(e) => {
                    if (isReorderMode) return;
                    e.preventDefault();
                    onDelete(note.id);
                }}
                {...(isReorderMode ? listeners : {})}
            >
                <div className="flex justify-between items-center">
                    <div className="flex items-center gap-1 min-w-0">
                        {isReorderMode && (
                            <MoveVertical className="h-4 w-4 text-secondary shrink-0" />
                        )}
                        <span className="font-medium truncate" title={note.title}>{note.title}</span>
                    </div>
                    {!isReorderMode && (
                        <button
                            type="button"
                            data-testid="notes-delete"
                            className="btn btn-xs btn-ghost btn-circle opacity-0 hover:opacity-100 focus-visible:opacity-100 shrink-0"
                            onClick={(e) => {
                                e.stopPropagation();
                                onDelete(note.id);
                            }}
                            aria-label={t("notes.delete")}
                        >
                            <X className="h-3 w-3" />
                        </button>
                    )}
                </div>
                <div className="flex justify-between flex-row items-center overflow-y-hidden">
                    <span className="text-xs flex-1 text-base-content/70 truncate text-left min-w-0">{contentPreview}</span>
                    <span className="text-xs text-base-content/70 shrink-0 ml-2">{formattedDate}</span>
                </div>
            </button>
        </div>
    );
});
