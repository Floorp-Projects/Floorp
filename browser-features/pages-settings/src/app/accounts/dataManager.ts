import { getAccountImage, getAccountInfo } from "@/app/dashboard/dataManager";
import type { AccountsFormData } from "@/types/pref";

export async function getCurrentProfile(): Promise<{
  profileName: string;
  profilePath: string;
}> {
  return await new Promise((resolve) => {
    window.NRGetCurrentProfile((data: unknown) => {
      // Handle null or undefined data
      if (!data) {
        resolve({ profileName: "", profilePath: "" });
        return;
      }

      let profileInfo: { profileName: string; profilePath: string } | null = null;

      if (typeof data === "string") {
        try {
          profileInfo = JSON.parse(data) as { profileName: string; profilePath: string };
        } catch {
          // Fallback to a safe default if JSON parsing fails
          resolve({ profileName: "", profilePath: "" });
          return;
        }
      } else if (
        typeof data === "object" &&
        data !== null &&
        "profileName" in data &&
        "profilePath" in data
      ) {
        profileInfo = data as { profileName: string; profilePath: string };
      }

      if (!profileInfo) {
        resolve({ profileName: "", profilePath: "" });
        return;
      }

      resolve(profileInfo);
    });
  });
}

export async function useAccountAndProfileData(): Promise<AccountsFormData> {
  const accountInfo = await getAccountInfo();
  const accountImage = await getAccountImage();
  const profileInfo = await getCurrentProfile();

  return {
    accountInfo: accountInfo,
    accountImage: accountImage,
    profileDir: profileInfo.profilePath,
    profileName: profileInfo.profileName,
    asyncNoesViaMozillaAccount: true,
  };
}
