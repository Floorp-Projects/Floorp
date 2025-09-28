// eslint-disable jsx-no-target-blank
import Navigation from "../../components/Navigation.tsx";
import { useTranslation } from "react-i18next";
import { setDefaultBrowser } from "@/app/finish/dataManager.ts";
import { useState } from "react";
import { Check, Sun, Github, Twitter, HelpCircle, ArrowRight } from "lucide-react";

export default function FinishPage() {
    const { t } = useTranslation();
    const [isDefaultBrowser, setIsDefaultBrowser] = useState({ message: "", success: false });

    const setAsDefaultBrowser = () => {
        setDefaultBrowser().then((success) => {
            if (success) {
                setIsDefaultBrowser({ message: t('finishPage.defaultBrowserSuccess'), success: true });
            } else {
                setIsDefaultBrowser({ message: t('finishPage.defaultBrowserError'), success: false });
            }
        });
    };

    const closeWelcomePage = () => {
        window.open("about:newtab", "_blank");
        setTimeout(() => {
            window.close();
        }, 50);
    };

    return (
        <main className="flex flex-col justify-between overflow-hidden py-6">
            <div className="container mx-auto px-4 z-10 flex flex-col items-center flex-grow">
                <div className="flex flex-col items-center justify-center mb-6 mt-2">
                    <div className="relative mb-5">
                        <div className="w-20 h-20 md:w-24 md:h-24 rounded-full bg-gradient-to-br from-success to-success/70 flex items-center justify-center shadow-lg shadow-success/30 animate-pulse">
                            <Check className="h-12 w-12 md:h-14 md:w-14 text-white" />
                        </div>
                        <div className="absolute inset-0 rounded-full border-3 border-dashed border-success/40 animate-[spin_10s_linear_infinite]"></div>
                    </div>

                    <h1 id="completion-title" className="text-3xl md:text-4xl lg:text-5xl font-bold text-center text-success mb-4">
                        {t('finishPage.title')}
                    </h1>

                    <p className="text-base md:text-lg text-center max-w-xl mb-5">
                        {t('finishPage.subtitle')}
                    </p>
                </div>

                <div className="card bg-base-100 shadow-xl w-full max-w-2xl backdrop-blur-sm bg-base-100/80 border border-base-300 hover:shadow-2xl transition-all duration-300 mb-6">
                    <div className="card-body p-4 md:p-6">
                        <h2 className="flex items-center gap-2 text-lg md:text-xl font-semibold text-primary mb-3">
                            <Sun size={20} />
                            {t('finishPage.nextSteps')}
                        </h2>

                        {isDefaultBrowser.message && (
                            <div className={`alert ${isDefaultBrowser.success ? 'alert-success' : 'alert-error'} mb-3 py-2`}>
                                <div className="flex-1">
                                    <p className="font-semibold text-sm">{isDefaultBrowser.message}</p>
                                </div>
                            </div>
                        )}

                        <div className="flex flex-col gap-3">
                            <button
                                className="btn btn-primary btn-md md:btn-lg shadow-lg hover:shadow-xl transform hover:-translate-y-1 transition-all duration-300 w-full"
                                onClick={setAsDefaultBrowser}
                            >
                                <Check className="mr-2" size={18} />
                                {t('finishPage.defaultBrowser')}
                            </button>

                            <div className="divider text-xs opacity-70 my-1">{t('finishPage.resourcesLabel') || 'リソース'}</div>

                            <div className="flex flex-wrap gap-2 justify-center">
                                <a href="https://github.com/Floorp-Projects/Floorp" target="_blank" className="btn btn-outline btn-sm">
                                    <Github className="mr-1" size={14} />
                                    GitHub
                                </a>
                                <a href="https://twitter.com/floorp_browser" target="_blank" className="btn btn-outline btn-sm">
                                    <Twitter className="mr-1" size={14} />
                                    X（Twitter）
                                </a>
                                <a href="https://docs.floorp.app/docs/features/" target="_blank" className="btn btn-outline btn-sm">
                                    <HelpCircle className="mr-1" size={14} />
                                    {t('finishPage.support')}
                                </a>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="flex flex-col items-center">
                    <button
                        className="btn btn-success btn-md md:btn-lg shadow-xl hover:shadow-success/20 transform hover:-translate-y-1 transition-all duration-300 px-6 md:px-8 text-white font-bold"
                        onClick={closeWelcomePage}
                    >
                        <ArrowRight className="mr-2" size={20} />
                        {t('finishPage.getStarted')}
                    </button>

                    <p className="text-xs md:text-sm opacity-70 mt-3 text-center max-w-md px-4">
                        {t('finishPage.hint')}
                    </p>
                </div>
            </div>

            <div className="mt-6">
                <Navigation />
            </div>

            <style dangerouslySetInnerHTML={{
                __html: `
                @keyframes fadeIn {
                    from { opacity: 0; transform: translateY(20px); }
                    to { opacity: 1; transform: translateY(0); }
                }
            `}} />
        </main>
    );
}
