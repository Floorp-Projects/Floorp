export const NimbusRolloutMessageProvider = {
  getMessages() {
    return [
      {
        content: {
          backdrop: "transparent",
          id: "test-message-import-infreq-make-self-at-home:treatment-c",
          screens: [
            {
              content: {
                logo: {
                  height: "186px",
                  imageURL:
                    "chrome://activity-stream/content/data/content/assets/person-typing.svg",
                },
                primary_button: {
                  action: {
                    navigate: true,
                    type: "SHOW_MIGRATION_WIZARD",
                  },
                  label: {
                    paddingBlock: "6px",
                    paddingInline: "14px",
                    string_id: "onboarding-infrequent-import-primary-button",
                  },
                },
                secondary_button: {
                  action: {
                    navigate: true,
                  },
                  label: {
                    fontSize: "13px",
                    lineHeight: "15px",
                    marginBlock: "-4px -28px",
                    string_id: "onboarding-not-now-button-label",
                  },
                },
                subtitle: {
                  fontSize: "13px",
                  letterSpacing: ".05px",
                  lineHeight: "16px",
                  marginBlock: "4px 12px",
                  paddingInline: "48px",
                  string_id: "onboarding-infrequent-import-subtitle",
                },
                title: {
                  fontSize: "26px",
                  fontWeight: "400",
                  letterSpacing: "-.01em",
                  lineHeight: "36px",
                  marginBlock: "6px 0",
                  string_id: "onboarding-infrequent-import-title",
                },
                title_style: "slim",
              },
              id: "IMPORT",
            },
          ],
          template: "multistage",
          transitions: true,
        },
        frequency: {
          lifetime: 1,
        },
        groups: ["import-spotlights"],
        id: "test-message-import-infreq-make-self-at-home:treatment-c",
        priority: 1,
        targeting:
          "!willShowDefaultPrompt && !('browser.migrate.content-modal.enabled'|preferenceValue) && source == 'startup' && !isMajorUpgrade && !activeNotifications && totalBookmarksCount == 5",
        template: "spotlight",
        trigger: {
          id: "defaultBrowserCheck",
        },
      },
    ];
  },
};
