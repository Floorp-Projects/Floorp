import type { Panel } from "../../../../chrome/common/panel-sidebar/utils/type.ts";
export type Container = {
  id: number | string;
  name: string;
  label: string;
  icon: string;
  color: string;
};

export type StaticPanel = {
  value: string;
  label: string;
  icon: string;
};

export type ExtensionPanel = {
  extensionId: string;
  title: string;
  iconUrl: string;
};

export interface PanelEditModalProps {
  panel: Panel;
  onSave: (panel: Panel) => void;
  onClose: () => void;
}
