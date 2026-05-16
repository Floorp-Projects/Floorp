import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { getWorkspaceSettings, saveWorkspaceSettings } from "./dataManager.ts";
import type { WorkspacesFormData } from "@/types/pref.ts";
import { LearnButton } from "@/components/common/learn-button.tsx";
import { HelpSection } from "@/components/common/help-section.tsx";
import type { TutorialStep } from "@/components/common/tutorial-modal.tsx";
import { WorkspaceGuide } from "@/components/common/workspace-guide.tsx";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<WorkspacesFormData>({});

  const workspaceTutorialSteps: TutorialStep[] = [
    {
      titleKey: "workspaces.tutorial.step1.title",
      descriptionKey: "workspaces.tutorial.step1.description",
      content: <WorkspaceGuide />,
    },
    {
      titleKey: "workspaces.tutorial.step2.title",
      descriptionKey: "workspaces.tutorial.step2.description",
      content: <WorkspaceGuide step={1} />,
    },
    {
      titleKey: "workspaces.tutorial.step3.title",
      descriptionKey: "workspaces.tutorial.step3.description",
    },
  ];

  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getWorkspaceSettings();
      if (!values) return;

      for (const key in values) {
        setValue(key as keyof WorkspacesFormData, values[key], {
          shouldValidate: true,
        });
      }
    };

    fetchDefaultValues();
    globalThis.addEventListener("focus", fetchDefaultValues);
    return () => {
      globalThis.removeEventListener("focus", fetchDefaultValues);
    };
  }, [setValue]);

  React.useEffect(() => {
    if (Object.keys(watchAll).length === 0) return;

    try {
      saveWorkspaceSettings(watchAll);
    } catch (error) {
      globalThis.console?.error("Failed to save workspace settings:", error);
    }
  }, [watchAll]);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("workspaces.workspaces")}
        </h1>
        <p className="text-sm mb-4">{t("workspaces.workspacesDescription")}</p>
        <div className="flex items-center gap-2">
          <LearnButton steps={workspaceTutorialSteps} title={t("workspaces.workspaces")} />
        </div>
        <div className="w-full max-w-2xl mb-6">
          <HelpSection summary={t("workspaces.helpArchiveWorkspaces")}>
            <p>{t("workspaces.helpArchiveWorkspacesDescription")}</p>
          </HelpSection>
        </div>
      </div>

      <FormProvider {...methods}>
        <form
          className="space-y-3 pl-6"
          onSubmit={(e) => e.preventDefault()}
        >
          <BasicSettings />
          {/* <BackupSettings /> */}
        </form>
      </FormProvider>
    </div>
  );
}
