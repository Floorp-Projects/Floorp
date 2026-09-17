import type { i18n as I18nInstance } from "i18next";
import type { SectionDefinition, SettingsSearchDocument } from "./types.ts";
import { SETTING_FIELDS } from "./fields.ts";
export type { SettingsSearchDocument } from "./types.ts";
import {
  AppWindow,
  BadgeInfo,
  Briefcase,
  Gauge,
  House,
  Keyboard,
  List,
  MousePointer,
  PanelLeft,
  PencilRuler,
  Sparkles,
  UserRoundPen,
} from "lucide-react";

const PREVIEW_LENGTH = 180;

const SECTION_DEFINITIONS: SectionDefinition[] = [
  {
    id: "overview-home",
    route: "/overview/home",
    titleKey: "pages.home",
    descriptionKey: "home.description",
    icon: House,
    priority: 90,
    textKey: "home",
  },
  {
    id: "design-tab-and-appearance",
    route: "/features/design",
    titleKey: "pages.tabAndAppearance",
    descriptionKey: "design.customizePositionOfToolbars",
    icon: PencilRuler,
    priority: 100,
    textKey: "design",
  },
  {
    id: "design-lepton",
    route: "/features/design/lepton",
    titleKey: "design.lepton-preferences.title",
    descriptionKey: "design.lepton-preferences.description",
    icon: Sparkles,
    priority: 60,
    textKey: "design.lepton-preferences",
  },
  {
    id: "panel-sidebar",
    route: "/features/sidebar",
    titleKey: "pages.browserSidebar",
    descriptionKey: "panelSidebar.description",
    icon: PanelLeft,
    priority: 85,
    textKey: "panelSidebar",
  },
  {
    id: "panel-sidebar-panels",
    route: "/features/sidebar",
    titleKey: "panelSidebar.panelList",
    descriptionKey: "panelSidebar.howToUseAndCustomize",
    icon: List,
    priority: 40,
    textKey: "panelSidebar",
  },
  {
    id: "workspaces",
    route: "/features/workspaces",
    titleKey: "pages.workspaces",
    descriptionKey: "workspaces.workspacesDescription",
    icon: Briefcase,
    priority: 80,
    textKey: "workspaces",
  },
  {
    id: "mouse-gestures",
    route: "/features/gesture",
    titleKey: "pages.mouseGesture",
    descriptionKey: "mouseGesture.description",
    icon: MousePointer,
    priority: 70,
    textKey: "mouseGesture",
  },
  {
    id: "keyboard-shortcuts",
    route: "/features/shortcuts",
    titleKey: "pages.keyboardShortcut",
    descriptionKey: "keyboardShortcut.description",
    icon: Keyboard,
    priority: 75,
    textKey: "keyboardShortcut",
  },
  {
    id: "web-apps",
    route: "/features/webapps",
    titleKey: "pages.webApps",
    descriptionKey: "progressiveWebApp.description",
    icon: AppWindow,
    priority: 65,
    textKey: "progressiveWebApp",
  },
  {
    id: "performance",
    route: "/features/performance",
    titleKey: "pages.performance",
    descriptionKey: "performance.description",
    icon: Gauge,
    priority: 55,
    textKey: "performance",
  },
  {
    id: "accounts",
    route: "/features/accounts",
    titleKey: "pages.profileAndAccount",
    descriptionKey: "accounts.profileDescription",
    icon: UserRoundPen,
    priority: 60,
    textKey: "accounts",
  },
  {
    id: "about-browser",
    route: "/about/browser",
    titleKey: "pages.aboutBrowser",
    descriptionKey: "about.browserDescription",
    icon: BadgeInfo,
    priority: 50,
    textKey: "about",
  },
  {
    id: "updates",
    route: "/about/updates",
    titleKey: "pages.updates",
    icon: Sparkles,
    priority: 50,
    textKey: "updates",
  },
  {
    id: "floorp-os",
    route: "/features/floorp-os",
    titleKey: "floorpOS.title",
    icon: AppWindow,
    priority: 40,
    textKey: "floorpOS",
  },
];

function sanitizeWhitespace(value: string): string {
  return value.replace(/\s+/g, " ").trim();
}

// Search indexes translated labels and descriptions without mounting feature UI.
// This keeps search independent of providers, IPC and route chunk loading.
function translatedSectionText(key: string, instance: I18nInstance): string {
  const keys = new Set<string>();
  const collect = (value: unknown, path: string): void => {
    if (typeof value === "string") {
      keys.add(path);
    } else if (value && typeof value === "object") {
      for (const [child, entry] of Object.entries(value)) {
        collect(entry, path + "." + child);
      }
    }
  };
  // Include keys missing from a partial locale; t() resolves each leaf using
  // the same fallback chain as the visible settings labels.
  const defaultNamespace = instance.options.defaultNS;
  const namespace = Array.isArray(defaultNamespace)
    ? defaultNamespace[0]
    : defaultNamespace || "translation";
  for (const language of instance.languages) {
    collect(instance.getResource(language, namespace, key), key);
  }
  return sanitizeWhitespace(
    [...keys].map((path) => String(instance.t(path))).join(" "),
  );
}

export function normalizeSearchText(value: string): string {
  return value
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase()
    .replace(/\s+/g, " ")
    .trim();
}

export function buildSearchDocuments(
  i18nInstance: I18nInstance,
): SettingsSearchDocument[] {
  const definitions: SectionDefinition[] = [
    ...SECTION_DEFINITIONS,
    ...SETTING_FIELDS.filter((field) => i18nInstance.exists(field.titleKey))
      .map((field) => ({
        ...field,
        id: `field:${field.route}:${field.id}`,
        route: `${field.route}?setting=${encodeURIComponent(field.id)}`,
        priority: 110,
        textKey: field.titleKey,
      })),
  ];
  return definitions.map((definition) => {
    const title = i18nInstance.t(definition.titleKey, {
      defaultValue: definition.titleKey,
    });
    const description = definition.descriptionKey
      ? i18nInstance.t(definition.descriptionKey, { defaultValue: "" })
      : "";

    const renderedText = translatedSectionText(
      definition.textKey,
      i18nInstance,
    );
    const textContent = renderedText ||
      sanitizeWhitespace(`${description} ${title}`);
    const preview = (definition.id.startsWith("field:") ? description : description || renderedText || title).slice(
      0,
      PREVIEW_LENGTH,
    );

    return {
      id: definition.id,
      route: definition.route,
      title,
      icon: definition.icon,
      priority: definition.priority,
      textContent,
      preview,
      normalizedTitle: normalizeSearchText(title),
      normalizedText: normalizeSearchText(textContent),
    } satisfies SettingsSearchDocument;
  });
}
