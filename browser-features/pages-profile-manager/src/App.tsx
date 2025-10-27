import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  createProfileWizard,
  getCurrentProfile,
  getFxAccountsInfo,
  getProfiles,
  openProfile,
} from "./lib/profileDataManager.ts";
import { Plus, RefreshCw, ShareIcon } from "lucide-react";

type ProfileItem = {
  name: string;
  rootDir?: { path: string };
  localDir?: { path: string };
  isDefault?: boolean;
  isCurrentProfile?: boolean;
  isInUse?: boolean;
};

function App() {
  const [profiles, setProfiles] = useState<ProfileItem[]>([]);
  const [loading, setLoading] = useState(false);
  const { t } = useTranslation();

  async function loadAll() {
    setLoading(true);
    try {
      const [p, _c] = await Promise.all([
        getProfiles(),
        getFxAccountsInfo(),
        getCurrentProfile(),
      ]);
      const profilesArray = Array.isArray(p) ? (p as ProfileItem[]) : [];
      setProfiles(profilesArray);
    } catch (err) {
      console.error("[App] loadAll: failed to load data", err);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    loadAll();
  }, []);

  function handleOpenProfile(id: string) {
    openProfile(id);
  }

  function handleCreate() {
    createProfileWizard();
    // wait a bit and refresh
    setTimeout(loadAll, 800);
  }

  return (
    <div className="min-h-screen p-6 bg-base-200">
      <div className="container mx-auto">
        <div className="flex flex-row sm:flex-row sm:items-center justify-between gap-2 mb-4">
          <h1 className="text-lg font-semibold">{t("profileManager.title")}</h1>
          <button
            type="button"
            aria-label={t("profileManager.buttons.create")}
            title={t("profileManager.buttons.create")}
            className="ml-auto btn btn-primary btn-sm flex items-center gap-2"
            onClick={handleCreate}
          >
            <Plus size={16} />
            <span>{t("profileManager.buttons.create")}</span>
          </button>

          <button
            type="button"
            aria-label={t("profileManager.buttons.refresh")}
            title={t("profileManager.buttons.refresh")}
            className="btn btn-secondary btn-sm flex items-center gap-2"
            onClick={loadAll}
          >
            <RefreshCw size={16} />
            <span>{t("profileManager.buttons.refresh")}</span>
          </button>
        </div>

        <div className="flex-1 mt-6 overflow-auto space-y-3 min-h-[60vh]">
          <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
            {loading && (
              <div className="col-span-full">{t("profileManager.loading")}</div>
            )}
            {!loading && profiles.length === 0 && (
              <div className="col-span-full">
                {t("profileManager.noProfiles")}
              </div>
            )}

            {profiles.map((p) => (
              <div
                key={p.name}
                className={`card card-compact bg-base-100 shadow`}
              >
                <div className="card-body p-3  flex flex-row justify-between">
                  <h2 className="card-title items-center gap-2 text-sm">
                    <span className="truncate text-sm">{p.name}</span>
                    {p.isCurrentProfile
                      ? (
                        <span className="badge badge-accent text-xs">
                          {t("profileManager.current")}
                        </span>
                      )
                      : null}
                  </h2>
                  <div className="card-actions flex flex-col justify-end gap-1">
                    <button
                      type="button"
                      className="btn btn-sm btn-primary w-full"
                      onClick={() => handleOpenProfile(p.name)}
                    >
                      <ShareIcon size={16} />
                      {t("profileManager.actions.open")}
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
