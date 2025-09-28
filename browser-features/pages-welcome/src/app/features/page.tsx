import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";
import panelSidebarSvg from "./assets/panelsidebar.svg";
import workspacesSvg from "./assets/workspaces.svg";
import pwaSvg from "./assets/pwa.svg";
import mouseGestureSvg from "./assets/mousegesture.svg";
import { PanelLeft, LayoutGrid, Globe, MousePointer } from "lucide-react";

export default function FeaturesPage() {
    const { t } = useTranslation();

    const panelSidebarImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
                <img src={panelSidebarSvg} alt="Panel Sidebar" className="w-full h-full object-contain" />
            </div>
        </div>
    );

    const workspaceImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
                <img src={workspacesSvg} alt="Workspaces" className="w-full h-full object-contain" />
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
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden">
                <img src={mouseGestureSvg} alt="Mouse Gesture" className="w-full h-full object-contain" />
            </div>
        </div>
    );

    const getTranslatedFeatures = (path: string): string[] => {
        const features = t(path, { returnObjects: true });
        return Array.isArray(features) ? features : [];
    };

    const panelSidebarFeatures = getTranslatedFeatures('featuresPage.panelSidebar.features');
    const workspacesFeatures = getTranslatedFeatures('featuresPage.workspaces.features');
    const pwaFeatures = getTranslatedFeatures('featuresPage.pwa.features');
    const mouseGestureFeatures = getTranslatedFeatures('featuresPage.mouseGesture.features');

    return (
        <div className="flex flex-col gap-8">
            <div className="text-center">
                <h1 className="text-4xl font-bold mb-4">{t('featuresPage.title')}</h1>
                <p className="text-lg">{t('featuresPage.subtitle')}</p>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div className="card bg-base-200 shadow-xl h-full">
                    <div className="card-body">
                        <div className="flex items-center mb-4">
                            <div className="text-primary mr-3">
                                <PanelLeft size={28} />
                            </div>
                            <h2 className="text-2xl font-bold">{t('featuresPage.panelSidebar.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.panelSidebar.description')}
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
                            <h2 className="text-2xl font-bold">{t('featuresPage.workspaces.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.workspaces.description')}
                                </p>
                                <ul className="list-disc list-inside space-y-1">
                                    {workspacesFeatures.map((feature, index) => (
                                        <li key={index}>{feature}</li>
                                    ))}
                                </ul>
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
                            <h2 className="text-2xl font-bold">{t('featuresPage.pwa.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.pwa.description')}
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
                            <h2 className="text-2xl font-bold">{t('featuresPage.mouseGesture.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.mouseGesture.description')}
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
            </div>

            <Navigation />
        </div>
    );
} 