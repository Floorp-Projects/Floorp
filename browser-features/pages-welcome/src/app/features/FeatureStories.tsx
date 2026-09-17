import { Tabs } from "@chakra-ui/react";
import { useState } from "react";
import { DemoPlayback } from "./DemoPlayback.tsx";
import { AppWindow, Layers, Mouse, PanelRight } from "lucide-react";
import { useTranslation } from "react-i18next";
import {
  AppDemo,
  GestureDemo,
  PanelDemo,
  WorkspaceDemo,
} from "./FeatureDemos.tsx";
import demoStyles from "./demos.module.css";
import styles from "../../setup.module.css";
const features = [
  { key: "workspaces", demo: WorkspaceDemo, icon: Layers },
  { key: "panelSidebar", demo: PanelDemo, icon: PanelRight },
  { key: "pwa", demo: AppDemo, icon: AppWindow },
  { key: "mouseGesture", demo: GestureDemo, icon: Mouse },
] as const;
export function FeatureStories() {
  const { t } = useTranslation();
  const [active, setActive] = useState("workspaces");
  const [playbackRuns, setPlaybackRuns] = useState<Record<string, number>>({});
  return (
    <Tabs.Root
      unstyled
      value={active}
      onValueChange={({ value }) => setActive(value)}
      lazyMount
      className={demoStyles.stories}
    >
      <Tabs.List
        className={styles.tabsList}
        aria-label={t("setupV5.featuresTitle")}
      >
        {features.map(({ key, icon: Icon }) => (
          <Tabs.Trigger key={key} value={key} className={styles.tab}>
            <Icon size={20} aria-hidden="true" />
            {key === "pwa"
              ? t("featureDemo.pwa.tab")
              : t(`featuresPage.${key}.title`)}
          </Tabs.Trigger>
        ))}
      </Tabs.List>
      {features.map(({ key, demo: Demo }, index) => {
        return (
          <Tabs.Content key={key} value={key} className={demoStyles.feature}>
            <p className={demoStyles.eyebrow}>{t("featureDemo.label")}</p>
            <h3>{t(`featureDemo.${key}.title`)}</h3>
            <DemoPlayback
              key={`${key}-${playbackRuns[key] ?? 0}`}
              active={active === key}
              onComplete={() => {
                const next = features[index + 1];
                if (!next) {
                  return;
                }
                setPlaybackRuns((runs) => ({
                  ...runs,
                  [next.key]: (runs[next.key] ?? 0) + 1,
                }));
                setActive(next.key);
              }}
            >
              <Demo />
            </DemoPlayback>
            <p className={demoStyles.benefit}>
              {t(`featureDemo.${key}.benefit`)}
            </p>
          </Tabs.Content>
        );
      })}
    </Tabs.Root>
  );
}
