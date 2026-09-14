import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Accounts } from "./components/Accounts.tsx";
import { Profile } from "./components/Profile.tsx";
import { FormProvider, useForm } from "react-hook-form";
import { useAccountAndProfileData } from "./dataManager.ts";
import type { AccountsFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<AccountsFormData>({
    defaultValues: {},
  });

  const [accountAndProfileData, setAccountAndProfileData] = useState<
    AccountsFormData | null
  >(null);

  useEffect(() => {
    async function fetchAccountAndProfileData() {
      const data = await useAccountAndProfileData();
      setAccountAndProfileData(data);
    }
    fetchAccountAndProfileData();
  }, []);

  return (
    <div className="floorp-settings-page">
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">
          {t("accounts.profileAndAccount")}
        </h1>
        <p className="floorp-page-description">{t("accounts.profileDescription")}</p>
      </div>

      <div className="floorp-settings-sections">
        <FormProvider {...methods}>
          <Accounts accountAndProfileData={accountAndProfileData} />
          <Profile accountAndProfileData={accountAndProfileData} />
        </FormProvider>
      </div>
    </div>
  );
}
