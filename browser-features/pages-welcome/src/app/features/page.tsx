import { useCallback } from "react";
import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";
import { rpc } from "../../lib/rpc/rpc.ts";
import panelSidebarSvg from "./assets/panelsidebar.svg";
import workspacesSvg from "./assets/workspaces.svg";
import pwaSvg from "./assets/pwa.svg";
import {
  Columns2,
  Globe,
  LayoutGrid,
  MousePointer,
  PanelLeft,
} from "lucide-react";

export default function FeaturesPage() {
  const { t } = useTranslation();

  const handleStartWorkspacesTour = useCallback(async () => {
    const payload = JSON.stringify({ tourId: "workspaces" });
    await rpc.setStringPref("floorp.guidedTour.request", payload);
  }, []);

  const panelSidebarImage = (
    <div className="w-full h-full flex items-center justify-center">
      <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
        <img
          src={panelSidebarSvg}
          alt="Panel Sidebar"
          className="w-full h-full object-contain"
        />
      </div>
    </div>
  );

  const workspaceImage = (
    <div className="w-full h-full flex items-center justify-center">
      <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
        <img
          src={workspacesSvg}
          alt="Workspaces"
          className="w-full h-full object-contain"
        />
      </div>
    </div>
  );

  const pwaImage = (
    <div className="w-full h-full flex items-center justify-center">
      <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
        <img src={pwaSvg} alt="PWA" className="w-full h-full object-contain" />
      </div>
    </div>
  );

  const mouseGestureImage = (
    <div className="w-full h-full flex items-center justify-center">
      <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden relative flex items-center justify-center">
        <svg viewBox="0 0 80 60" className="w-full h-full" fill="none">
          <path
            d="M 20 30 L 20 10 L 50 10 L 50 30"
            stroke="currentColor"
            strokeWidth="2.5"
            strokeLinecap="round"
            strokeLinejoin="round"
            className="text-primary"
            style={{
              strokeDasharray: 80,
              strokeDashoffset: 80,
              animation: "gesture-draw 2s ease-in-out infinite",
            }}
          />
          <circle
            cx="20"
            cy="30"
            r="3"
            className="fill-primary"
            style={{ animation: "gesture-dot 2s ease-in-out infinite" }}
          />
        </svg>
        <style>
          {`
                    @keyframes gesture-draw {
                        0% { stroke-dashoffset: 80; }
                        50% { stroke-dashoffset: 0; }
                        100% { stroke-dashoffset: -80; }
                    }
                    @keyframes gesture-dot {
                        0%, 10% { opacity: 1; }
                        50%, 60% { opacity: 0; }
                        100% { opacity: 1; }
                    }
                `}
        </style>
      </div>
    </div>
  );

  const splitviewImage = (
    <div className="w-full h-full flex items-center justify-center">
      <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden relative">
        <div className="flex w-full h-full gap-[2px] rounded overflow-hidden">
          <div className="bg-base-100 rounded-l flex-1 flex flex-col p-1.5 gap-1 animate-splitview-left">
            <div className="h-2 w-3/4 bg-base-300 rounded" />
            <div className="h-1 w-full bg-base-200 rounded" />
            <div className="h-1 w-5/6 bg-base-200 rounded" />
            <div className="h-1 w-full bg-base-200 rounded" />
          </div>
          <div className="bg-base-100 rounded-r flex-1 flex flex-col p-1.5 gap-1 animate-splitview-right">
            <div className="h-2 w-2/3 bg-base-300 rounded" />
            <div className="h-1 w-full bg-base-200 rounded" />
            <div className="h-1 w-4/5 bg-base-200 rounded" />
            <div className="h-1 w-full bg-base-200 rounded" />
          </div>
        </div>
      </div>
      <style>
        {`
                @keyframes splitview-left {
                    0%, 100% { flex: 1; }
                    50% { flex: 1.5; }
                }
                @keyframes splitview-right {
                    0%, 100% { flex: 1; }
                    50% { flex: 0.6; }
                }
                .animate-splitview-left {
                    animation: splitview-left 3s ease-in-out infinite;
                }
                .animate-splitview-right {
                    animation: splitview-right 3s ease-in-out infinite;
                }
            `}
      </style>
    </div>
  );

  const getTranslatedFeatures = (path: string): string[] => {
    const features = t(path, { returnObjects: true });
    if (!Array.isArray(features)) return [];
    return features.filter((feature): feature is string =>
      typeof feature === "string"
    );
  };

  const panelSidebarFeatures = getTranslatedFeatures(
    "featuresPage.panelSidebar.features",
  );
  const workspacesFeatures = getTranslatedFeatures(
    "featuresPage.workspaces.features",
  );
  const pwaFeatures = getTranslatedFeatures("featuresPage.pwa.features");
  const mouseGestureFeatures = getTranslatedFeatures(
    "featuresPage.mouseGesture.features",
  );
  const splitviewFeatures = getTranslatedFeatures(
    "featuresPage.splitView.features",
  );

  return (
    <div className="flex flex-col gap-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold mb-4">{t("featuresPage.title")}</h1>
        <p className="text-lg">{t("featuresPage.subtitle")}</p>
      </div>
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div className="card bg-base-200 shadow-xl h-full">
          <div className="card-body">
            <div className="flex items-center mb-4">
              <div className="text-primary mr-3">
                <PanelLeft size={28} />
              </div>
              <h2 className="text-2xl font-bold">
                {t("featuresPage.panelSidebar.title")}
              </h2>
            </div>
            <div className="flex flex-col md:flex-row gap-4 items-center">
              <div className="md:w-1/2">
                <p className="mb-3">
                  {t("featuresPage.panelSidebar.description")}
                </p>
                <ul className="list-disc list-inside space-y-1">
                  {panelSidebarFeatures.map((feature, index) => (
                    <li key={index}>{feature}</li>
                  ))}
                </ul>
              </div>
              <div className="md:w-1/2">
                {panelSidebarImage}
              </div>
            </div>
          </div>
        </div>

        <div className="card bg-base-200 shadow-xl h-full">
          <div className="card-body">
            <div className="flex items-center mb-4">
              <div className="text-primary mr-3">
                <LayoutGrid size={28} />
              </div>
              <h2 className="text-2xl font-bold">
                {t("featuresPage.workspaces.title")}
              </h2>
            </div>
            <div className="flex flex-col md:flex-row gap-4 items-center">
              <div className="md:w-1/2">
                <p className="mb-3">
                  {t("featuresPage.workspaces.description")}
                </p>
                <ul className="list-disc list-inside space-y-1">
                  {workspacesFeatures.map((feature, index) => (
                    <li key={index}>{feature}</li>
                  ))}
                </ul>
                <button
                  type="button"
                  className="btn btn-primary btn-sm mt-4"
                  onClick={handleStartWorkspacesTour}
                >
                  {t("featuresPage.workspaces.startTour")}
                </button>
              </div>
              <div className="md:w-1/2">
                {workspaceImage}
              </div>
            </div>
          </div>
        </div>

        <div className="card bg-base-200 shadow-xl h-full">
          <div className="card-body">
            <div className="flex items-center mb-4">
              <div className="text-primary mr-3">
                <Globe size={28} />
              </div>
              <h2 className="text-2xl font-bold">
                {t("featuresPage.pwa.title")}
              </h2>
            </div>
            <div className="flex flex-col md:flex-row gap-4 items-center">
              <div className="md:w-1/2">
                <p className="mb-3">
                  {t("featuresPage.pwa.description")}
                </p>
                <ul className="list-disc list-inside space-y-1">
                  {pwaFeatures.map((feature, index) => (
                    <li key={index}>{feature}</li>
                  ))}
                </ul>
              </div>
              <div className="md:w-1/2">
                {pwaImage}
              </div>
            </div>
          </div>
        </div>

        <div className="card bg-base-200 shadow-xl h-full">
          <div className="card-body">
            <div className="flex items-center mb-4">
              <div className="text-primary mr-3">
                <MousePointer size={28} />
              </div>
              <h2 className="text-2xl font-bold">
                {t("featuresPage.mouseGesture.title")}
              </h2>
            </div>
            <div className="flex flex-col md:flex-row gap-4 items-center">
              <div className="md:w-1/2">
                <p className="mb-3">
                  {t("featuresPage.mouseGesture.description")}
                </p>
                <ul className="list-disc list-inside space-y-1">
                  {mouseGestureFeatures.map((feature, index) => (
                    <li key={index}>{feature}</li>
                  ))}
                </ul>
              </div>
              <div className="md:w-1/2">
                {mouseGestureImage}
              </div>
            </div>
          </div>
        </div>

        <div className="card bg-base-200 shadow-xl h-full">
          <div className="card-body">
            <div className="flex items-center mb-4">
              <div className="text-primary mr-3">
                <Columns2 size={28} />
              </div>
              <h2 className="text-2xl font-bold">
                {t("featuresPage.splitView.title")}
              </h2>
            </div>
            <div className="flex flex-col md:flex-row gap-4 items-center">
              <div className="md:w-1/2">
                <p className="mb-3">
                  {t("featuresPage.splitView.description")}
                </p>
                <ul className="list-disc list-inside space-y-1">
                  {splitviewFeatures.map((feature, index) => (
                    <li key={index}>{feature}</li>
                  ))}
                </ul>
              </div>
              <div className="md:w-1/2">
                {splitviewImage}
              </div>
            </div>
          </div>
        </div>
      </div>

      <Navigation />
    </div>
  );
}
