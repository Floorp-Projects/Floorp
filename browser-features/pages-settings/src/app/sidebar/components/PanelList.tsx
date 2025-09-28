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
} from "../../../../../../apps/main/core/common/panel-sidebar/utils/type.ts";
import { PanelEditModal } from "./PanelEditModal.tsx";
import {
  AlertTriangle,
  Edit,
  GripVertical,
  List,
  Plus,
  Trash2,
} from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "../../../../../newtab/src/components/common/card.tsx";

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
      className="bg-base-100 hover:bg-base-200 p-4 flex items-center justify-between transition-colors"
    >
      <div className="flex items-center gap-3">
        <div
          {...attributes}
          {...listeners}
          className="cursor-grab text-base-content/50 hover:text-base-content/70"
          aria-label="Drag to reorder"
        >
          <GripVertical size={20} />
        </div>
        <div>
          <div className="font-medium flex items-center gap-2">
            {panel.icon && (
              <img src={panel.icon} alt="" className="w-5 h-5 rounded-full" />
            )}
            <span className="max-w-md truncate">
              {displayName}
            </span>
          </div>
          <div className="text-sm text-base-content/60 mt-1">
            <div className="badge badge-sm">
              {t(`panelSidebar.type.${panel.type}`)}
            </div>
            {panel.width > 0 && (
              <span className="ml-2 text-xs">{panel.width}px</span>
            )}
          </div>
        </div>
      </div>
      <div className="flex gap-2">
        <button
          type="button"
          onClick={() => onEdit(panel)}
          className="btn btn-ghost btn-sm btn-square"
          aria-label={t("common.edit")}
        >
          <Edit size={16} />
        </button>
        <button
          onClick={() => onDelete(panel.id)}
          className="btn btn-ghost btn-sm btn-square"
          aria-label={t("common.delete")}
        >
          <Trash2 size={16} />
        </button>
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
    <div className="modal modal-open">
      <div className="modal-box">
        <h3 className="font-bold text-lg flex items-center gap-2">
          <AlertTriangle className="h-5 w-5 text-warning" />
          {t("panelSidebar.confirmDeleteTitle")}
        </h3>
        <p className="py-4">
          {t("panelSidebar.confirmDelete", { name: panelName })}
        </p>
        <div className="modal-action">
          <button onClick={onClose} className="btn btn-outline">
            {t("panelSidebar.cancel")}
          </button>
          <button onClick={onConfirm} className="btn btn-error">
            {t("panelSidebar.delete")}
          </button>
        </div>
      </div>
      <div className="modal-backdrop" onClick={onClose}></div>
    </div>
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

  const handleDragEnd = async (event: DragEndEvent) => {
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
        <span className="loading loading-spinner loading-md text-primary">
        </span>
      </div>
    );
  }

  return (
    <Card className="bg-base-200">
      <CardHeader className="flex flex-row justify-between">
        <CardTitle className="flex gap-2">
          <List className="size-5" />
          {t("panelSidebar.panelList")}
        </CardTitle>
        <div className="flex justify-end">
          <button
            type="button"
            onClick={handleAddPanel}
            className="btn btn-primary btn-sm gap-2"
          >
            <Plus size={16} />
            {t("panelSidebar.addPanel")}
          </button>
        </div>
      </CardHeader>
      <CardContent>
        {panels.length === 0
          ? (
            <div className="card-body items-center text-center text-base-content/70 py-8 bg-base-100 rounded-lg">
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
