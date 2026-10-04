export interface DismissTestUri {
  spec: string;
}

export interface DismissTestChromeHost {
  ChromeUtils: {
    importESModule(url: string): {
      NRWelcomePageParent: {
        prototype: {
          receiveMessage(
            this: unknown,
            message: { name: string },
          ): Promise<void>;
        };
      };
    };
  };
  Services: {
    io: { newURI(url: string): DismissTestUri };
  };
}
