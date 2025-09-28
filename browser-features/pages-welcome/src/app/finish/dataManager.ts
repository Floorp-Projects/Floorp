export async function setDefaultBrowser(): Promise<boolean> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRSetDefaultBrowser((data: string) => {
      resolve(JSON.parse(data).success as boolean);
    });
  });
}
