import styles from "@/components/common/settings-sections.module.css";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import { Button } from "../../../../../../libs/ui/button.tsx";
import type React from "react";
import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  closestCenter,
  defaultDropAnimation,
  DndContext,
  type DragEndEvent,
  DragOverlay,
  type DragStartEvent,
  KeyboardSensor,
  type Modifier,
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
import {
  addPanel,
  deletePanel,
  getPanelsList,
  getStaticPanelDisplayName,
  savePanelsList,
  updatePanel,
} from "../dataManager.ts";
import type {
  Panel,
  Panels,
} from "#features-chrome/common/panel-sidebar/utils/type.ts";
import { PanelEditModal } from "./PanelEditModal.tsx";
import { Edit, GripVertical, List, Plus, Trash2 } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "../../../components/common/card.tsx";

const SortablePanel = ({
  panel,
  onEdit,
  onDelete,
}: {
  panel: Panel;
  onEdit: (panel: Panel) => void;
  onDelete: (id: string) => void;
}) => {
  const { t } = useTranslation();
  const { attributes, listeners, setNodeRef, transform, transition } =
    useSortable({ id: panel.id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

  const displayName = useMemo(() => {
    if (panel.type === "static") {
      return getStaticPanelDisplayName(panel.url as string, t);
    }
    return panel.url || panel.extensionId || t("panelSidebar.untitled");
  }, [panel, t]);

  return (
    <div
      ref={setNodeRef}
      style={style}
      className="bg-base-100 hover:bg-base-200 p-4 flex items-center justify-between gap-3 transition-colors"
    >
      <div className="flex min-w-0 items-center gap-3">
        <div
          {...attributes}
          {...listeners}
          className="shrink-0 cursor-grab text-base-content/50 hover:text-base-content/70"
          aria-label="Drag to reorder"
        >
          <GripVertical size={20} />
        </div>
        <div className="min-w-0">
          <div className="font-medium flex items-center gap-2">
            {panel.icon && (
              <img src={panel.icon} alt="" className="w-5 h-5 shrink-0 rounded-full" />
            )}
            <span className="max-w-md truncate">
              {displayName}
            </span>
          </div>
          <div className="text-sm text-base-content/60 mt-1">
            <div className="floorp-tag">
              {t(`panelSidebar.type.${panel.type}`)}
            </div>
            {panel.width > 0 && (
              <span className="ml-2 text-xs">{panel.width}px</span>
            )}
          </div>
        </div>
      </div>
      <div className="flex shrink-0 gap-2">
        <Button
          type="button"
          onClick={() => onEdit(panel)}
          variant="ghost"
          aria-label={t("panelSidebar.editPanel")}
        >
          <Edit size={16} />
        </Button>
        <Button
          type="button"
          onClick={() => onDelete(panel.id)}
          variant="ghost"
          aria-label={t("panelSidebar.delete")}
        >
          <Trash2 size={16} />
        </Button>
      </div>
    </div>
  );
};

const restrictToVerticalAxis: Modifier = ({ transform }) => {
  return {
    ...transform,
    x: 0,
  };
};

const DeleteConfirmationModal = ({
  isOpen,
  onClose,
  onConfirm,
  panelName,
}: {
  isOpen: boolean;
  onClose: () => void;
  onConfirm: () => void;
  panelName: string;
}) => {
  const { t } = useTranslation();

  if (!isOpen) return null;

  return (
    <ConfirmModal
      isOpen={isOpen}
      onClose={onClose}
      onConfirm={onConfirm}
      title={t("panelSidebar.confirmDeleteTitle")}
      cancelText={t("panelSidebar.cancel")}
      confirmText={t("panelSidebar.delete")}
      confirmVariant="danger"
    >
      <p>{t("panelSidebar.confirmDelete", { name: panelName })}</p>
    </ConfirmModal>
  );
};

export const PanelList: React.FC = () => {
  const { t } = useTranslation();
  const [panels, setPanels] = useState<Panels>([]);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [currentPanel, setCurrentPanel] = useState<Panel | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [panelToDelete, setPanelToDelete] = useState<Panel | null>(null);
  const [isDeleteModalOpen, setIsDeleteModalOpen] = useState(false);

  const sensors = useSensors(
    useSensor(PointerSensor, {
      activationConstraint: {
        distance: 8,
      },
    }),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    }),
  );

  useEffect(() => {
    const fetchPanels = async () => {
      setIsLoading(true);
      const data = await getPanelsList();
      if (data) {
        setPanels(data);
      }
      setIsLoading(false);
    };

    fetchPanels();
  }, []);

  const handleAddPanel = () => {
    const newPanel: Panel = {
      id: `panel-${Date.now()}`,
      type: "web",
      width: 300,
      url: "",
      icon: "",
      userContextId: null,
      zoomLevel: null,
      userAgent: null,
      extensionId: null,
    };

    setCurrentPanel(newPanel);
    setIsModalOpen(true);
  };

  const handleEditPanel = (panel: Panel) => {
    setCurrentPanel({ ...panel });
    setIsModalOpen(true);
  };

  const handleSavePanel = async (panel: Panel) => {
    setIsModalOpen(false);

    const isNew = !panels.some((p) => p.id === panel.id);

    if (isNew) {
      await addPanel(panel);
    } else {
      await updatePanel(panel);
    }

    const updatedPanels = await getPanelsList();
    if (updatedPanels) {
      setPanels(updatedPanels);
    }
  };

  const handleShowDeleteModal = (panel: Panel) => {
    setPanelToDelete(panel);
    setIsDeleteModalOpen(true);
  };

  const handleDeletePanel = async () => {
    if (!panelToDelete) return;

    await deletePanel(panelToDelete.id);
    const updatedPanels = await getPanelsList();
    if (updatedPanels) {
      setPanels(updatedPanels);
    }

    setIsDeleteModalOpen(false);
    setPanelToDelete(null);
  };

  const handleCloseModal = () => {
    setIsModalOpen(false);
    setCurrentPanel(null);
  };

  const handleDragStart = (event: DragStartEvent) => {
    setActiveId(event.active.id as string);
  };

  const handleDragEnd = (event: DragEndEvent) => {
    setActiveId(null);
    const { active, over } = event;

    if (over && active.id !== over.id) {
      setPanels((items) => {
        const oldIndex = items.findIndex((item) => item.id === active.id);
        const newIndex = items.findIndex((item) => item.id === over.id);
        const newItems = arrayMove(items, oldIndex, newIndex);
        savePanelsList(newItems);
        return newItems;
      });
    }
  };

  const closeDeleteModal = () => {
    setIsDeleteModalOpen(false);
    setPanelToDelete(null);
  };

  if (isLoading) {
    return (
      <div className="flex justify-center py-8">
        <span role="status">{t("ui.loading")}</span>
      </div>
    );
  }

  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle className="flex gap-2">
          <List className="size-5" />
          {t("panelSidebar.panelList")}
        </CardTitle>
      </CardHeader>
      <CardContent>
        <div className={styles.toolbar}>
          <Button
            type="button"
            onClick={handleAddPanel}
            variant="primary"
            className="gap-2"
          >
            <Plus size={16} />
            {t("panelSidebar.addPanel")}
          </Button>
        </div>
        {panels.length === 0
          ? (
            <div className="floorp-empty-state items-center text-center text-base-content/70 py-8 bg-base-100 rounded-lg">
              <p>{t("panelSidebar.noPanels")}</p>
            </div>
          )
          : (
            <DndContext
              sensors={sensors}
              collisionDetection={closestCenter}
              onDragStart={handleDragStart}
              onDragEnd={handleDragEnd}
              modifiers={[restrictToVerticalAxis, restrictToParentElement]}
            >
              <SortableContext
                items={panels}
                strategy={verticalListSortingStrategy}
              >
                <div className="divide-y divide-base-300 rounded-lg overflow-hidden bg-base-200">
                  {panels.map((panel) => (
                    <SortablePanel
                      key={panel.id}
                      panel={panel}
                      onEdit={handleEditPanel}
                      onDelete={() => handleShowDeleteModal(panel)}
                    />
                  ))}
                </div>
              </SortableContext>
              <DragOverlay
                dropAnimation={defaultDropAnimation}
              >
                {activeId
                  ? (
                    <SortablePanel
                      panel={panels.find((p) => p.id === activeId)!}
                      onEdit={handleEditPanel}
                      onDelete={() =>
                        handleShowDeleteModal(
                          panels.find((p) => p.id === activeId)!,
                        )}
                    />
                  )
                  : null}
              </DragOverlay>
            </DndContext>
          )}
      </CardContent>

      {isModalOpen && currentPanel && (
        <PanelEditModal
          panel={currentPanel}
          onSave={handleSavePanel}
          onClose={handleCloseModal}
        />
      )}

      <DeleteConfirmationModal
        isOpen={isDeleteModalOpen}
        onClose={closeDeleteModal}
        onConfirm={handleDeletePanel}
        panelName={panelToDelete
          ? panelToDelete.type === "static"
            ? getStaticPanelDisplayName(panelToDelete.url as string, t)
            : (panelToDelete.url || panelToDelete.extensionId ||
              t("panelSidebar.untitled"))
          : ""}
      />
    </Card>
  );
};
