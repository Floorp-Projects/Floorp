import type { ComponentType } from "react";
import type { i18n as I18nInstance } from "i18next";
import { renderToStaticMarkup } from "react-dom/server";
import { I18nextProvider } from "react-i18next";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import type { LucideIcon } from "lucide-react";
import {
  AppWindow,
  BadgeInfo,
  Briefcase,
  House,
  Keyboard,
  List,
  MousePointer,
  PanelLeft,
  PencilRuler,
  Sparkles,
  UserRoundPen,
} from "lucide-react";

import Dashboard from "@/app/dashboard/page.tsx";
import Design from "@/app/design/page.tsx";
import { LeptonSettings } from "@/app/design/components/LeptonSettings.tsx";
import PanelSidebar from "@/app/sidebar/page.tsx";
import Workspaces from "@/app/workspaces/page.tsx";
import ProgressiveWebApp from "@/app/pwa/page.tsx";
import ProfileAndAccount from "@/app/accounts/page.tsx";
import MouseGesture from "@/app/gesture/page.tsx";
import KeyboardShortcut from "@/app/keyboard-shortcut/page.tsx";
import Debug from "@/app/debug/page.tsx";
import About from "@/app/about/noraneko.tsx";

const PREVIEW_LENGTH = 180;

interface SectionDefinition {
  id: string;
  route: string;
  titleKey: string;
  descriptionKey?: string;
  icon?: LucideIcon;
  priority: number;
  Component: ComponentType;
}

const SECTION_DEFINITIONS: SectionDefinition[] = [
  {
    id: "overview-home",
    route: "/overview/home",
    titleKey: "pages.home",
    descriptionKey: "home.description",
    icon: House,
    priority: 90,
    Component: Dashboard,
  },
  {
    id: "design-tab-and-appearance",
    route: "/features/design",
    titleKey: "pages.tabAndAppearance",
    descriptionKey: "design.customizePositionOfToolbars",
    icon: PencilRuler,
    priority: 100,
    Component: Design,
  },
  {
    id: "design-lepton",
    route: "/features/design/lepton",
    titleKey: "design.lepton-preferences.title",
    descriptionKey: "design.lepton-preferences.description",
    icon: Sparkles,
    priority: 60,
    Component: LeptonSettings,
  },
  {
    id: "panel-sidebar",
    route: "/features/sidebar",
    titleKey: "pages.browserSidebar",
    descriptionKey: "panelSidebar.description",
    icon: PanelLeft,
    priority: 85,
    Component: PanelSidebar,
  },
  {
    id: "panel-sidebar-panels",
    route: "/features/sidebar",
    titleKey: "panelSidebar.panelList",
    descriptionKey: "panelSidebar.howToUseAndCustomize",
    icon: List,
    priority: 40,
    Component: PanelSidebar,
  },
  {
    id: "workspaces",
    route: "/features/workspaces",
    titleKey: "pages.workspaces",
    descriptionKey: "workspaces.workspacesDescription",
    icon: Briefcase,
    priority: 80,
    Component: Workspaces,
  },
  {
    id: "mouse-gestures",
    route: "/features/gesture",
    titleKey: "pages.mouseGesture",
    descriptionKey: "mouseGesture.description",
    icon: MousePointer,
    priority: 70,
    Component: MouseGesture,
  },
  {
    id: "keyboard-shortcuts",
    route: "/features/shortcuts",
    titleKey: "pages.keyboardShortcut",
    descriptionKey: "keyboardShortcut.description",
    icon: Keyboard,
    priority: 75,
    Component: KeyboardShortcut,
  },
  {
    id: "web-apps",
    route: "/features/webapps",
    titleKey: "pages.webApps",
    descriptionKey: "progressiveWebApp.description",
    icon: AppWindow,
    priority: 65,
    Component: ProgressiveWebApp,
  },
  {
    id: "accounts",
    route: "/features/accounts",
    titleKey: "pages.profileAndAccount",
    descriptionKey: "accounts.profileDescription",
    icon: UserRoundPen,
    priority: 60,
    Component: ProfileAndAccount,
  },
  {
    id: "about-browser",
    route: "/about/browser",
    titleKey: "pages.aboutBrowser",
    descriptionKey: "about.browserDescription",
    icon: BadgeInfo,
    priority: 50,
    Component: About,
  },
  {
    id: "debug",
    route: "/debug",
    titleKey: "pages.debug",
    icon: List,
    priority: 10,
    Component: Debug,
  },
];

function sanitizeWhitespace(value: string): string {
  return value.replace(/\s+/g, " ").trim();
}

function htmlToPlainText(markup: string): string {
  // Avoid using innerHTML or fragment parser in any environment (including
  // chrome:// or about: pages). Use a conservative HTML-to-text conversion
  // that strips tags and decodes simple HTML entities.

  // 1) Strip tags
  let text = markup.replace(/<[^>]+>/g, " ");

  // 2) Decode a few common HTML entities (minimal, safe replacement)
  //    This avoids depending on DOM APIs or external libraries.
  const entities: [RegExp, string][] = [
    [/&nbsp;/gi, " "],
    [/&lt;/gi, "<"],
    [/&gt;/gi, ">"],
    [/&amp;/gi, "&"],
    [/&quot;/gi, '"'],
    [/&#39;/gi, "'"],
  ];
  for (const [re, repl] of entities) {
    text = text.replace(re, repl);
  }

  // 3) Collapse whitespace and trim
  return sanitizeWhitespace(text);
}

function buildPreview(text: string): string {
  const trimmed = text.trim();
  if (!trimmed) return "";
  if (trimmed.length <= PREVIEW_LENGTH) {
    return trimmed;
  }
  return `${trimmed.slice(0, PREVIEW_LENGTH).trim()}…`;
}

function renderSectionToText(
  definition: SectionDefinition,
  i18nInstance: I18nInstance,
): string {
  const Component = definition.Component;
  try {
    const markup = renderToStaticMarkup(
      <I18nextProvider i18n={i18nInstance}>
        <MemoryRouter initialEntries={[definition.route]}>
          <Routes>
            <Route path="*" element={<Component />} />
          </Routes>
        </MemoryRouter>
      </I18nextProvider>,
    );
    return htmlToPlainText(markup);
  } catch (error) {
    console.error(
      `[settings-search] Failed to render section "${definition.id}":`,
      error,
    );
    return "";
  }
}

export function normalizeSearchText(value: string): string {
  return value
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase()
    .replace(/\s+/g, " ")
    .trim();
}

export interface SettingsSearchDocument {
  id: string;
  route: string;
  title: string;
  icon?: LucideIcon;
  priority: number;
  textContent: string;
  preview: string;
  normalizedTitle: string;
  normalizedText: string;
}

export function buildSearchDocuments(
  i18nInstance: I18nInstance,
): SettingsSearchDocument[] {
  return SECTION_DEFINITIONS.map((definition) => {
    const title = i18nInstance.t(definition.titleKey, {
      defaultValue: definition.titleKey,
    });
    const description = definition.descriptionKey
      ? i18nInstance.t(definition.descriptionKey, { defaultValue: "" })
      : "";

    const renderedText = renderSectionToText(definition, i18nInstance);
    const textContent = renderedText ||
      sanitizeWhitespace(`${description} ${title}`);
    const preview = buildPreview(renderedText || description || title);

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
