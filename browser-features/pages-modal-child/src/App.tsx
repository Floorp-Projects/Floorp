import {
  type Control,
  Controller,
  FormProvider,
  useForm,
} from "react-hook-form";
import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import type {
  TForm,
  TFormItem,
} from "../../../common/modal-parent/utils/type.ts";

interface FormValues {
  [key: string]: string;
}

declare global {
  // deno-lint-ignore no-var
  var buildFormFromConfig: (config: TForm) => void;
  // deno-lint-ignore no-var
  var sendForm: (data: FormValues | null) => void;
  // deno-lint-ignore no-var
  var removeForm: () => void;
}

interface FormFieldProps {
  item: TFormItem;
  control: Control<FormValues>;
}

const FormField = ({ item, control }: FormFieldProps) => {
  const [dropdownOpen, setDropdownOpen] = useState(false);
  const dropdownRef = useRef<HTMLDivElement>(null);
  const buttonRef = useRef<HTMLButtonElement>(null);
  const [dropdownStyle, setDropdownStyle] = useState<React.CSSProperties>({});
  const [portalContainer, setPortalContainer] = useState<HTMLElement | null>(
    null,
  );

  useEffect(() => {
    setPortalContainer(document.body);
  }, []);

  useLayoutEffect(() => {
    if (dropdownOpen && buttonRef.current) {
      const rect = buttonRef.current.getBoundingClientRect();
      const windowWidth = globalThis.innerWidth;
      const windowHeight = globalThis.innerHeight;
      const scrollY = globalThis.scrollY;
      const scrollX = globalThis.scrollX;

      // Calculate max height based on available space
      // Use smaller max height to ensure dropdown stays within modal bounds
      const buttonWidth = rect.width;
      const spaceBelow = windowHeight - rect.bottom;
      const spaceAbove = rect.top;

      // Use available space, but cap at reasonable maximum (250px)
      // This ensures dropdowns don't overflow the modal and look balanced
      const dropdownMaxHeight = Math.min(
        Math.max(spaceBelow, spaceAbove),
        250,
      );

      let top = rect.bottom + scrollY;
      let left = rect.left + scrollX;

      // If not enough space below, show above
      if (spaceBelow < dropdownMaxHeight && spaceAbove > dropdownMaxHeight) {
        top = rect.top + scrollY - dropdownMaxHeight;
      }

      const spaceRight = windowWidth - rect.left;

      if (spaceRight < buttonWidth) {
        left = rect.right + scrollX - buttonWidth;
        if (left < scrollX) {
          left = scrollX + 10;
        }
      }

      setDropdownStyle({
        position: "fixed",
        top: `${top}px`,
        left: `${left}px`,
        width: `${buttonWidth}px`,
        maxHeight: `${dropdownMaxHeight}px`,
        zIndex: 1000,
        scrollbarWidth: "thin",
        overflowY: "auto" as const,
      });
    }
  }, [dropdownOpen]);

  useEffect(() => {
    if (item.type !== "dropdown" || !dropdownOpen) return;
    const handleOutsideClick = (e: MouseEvent) => {
      if (
        dropdownRef.current &&
        !dropdownRef.current.contains(e.target as Node) &&
        buttonRef.current &&
        !buttonRef.current.contains(e.target as Node)
      ) {
        setDropdownOpen(false);
      }
    };
    document.addEventListener("click", handleOutsideClick);
    return () => {
      document.removeEventListener("click", handleOutsideClick);
    };
  }, [dropdownOpen, item.type]);

  const validateAndFormatUrl = (url: string): string => {
    if (!url) return url;

    if (/^https?:\/\//i.test(url)) {
      return url;
    }

    if (/^[a-zA-Z0-9][-a-zA-Z0-9]*(\.[a-zA-Z0-9][-a-zA-Z0-9]*)+$/.test(url)) {
      return `https://${url}`;
    }

    return url;
  };

  switch (item.type) {
    case "text":
    case "url":
    case "number":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <input
                {...field}
                type={item.type === "url" ? "text" : item.type}
                className={`w-full px-4 py-2
                ${item.type === "url" ? "need-url-validation" : ""}
                  text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
                  item.classList || ""
                }`}
                placeholder={item.placeholder || ""}
                maxLength={item.maxLength}
                onBlur={item.type === "url"
                  ? (e) => {
                    const formattedUrl = validateAndFormatUrl(
                      e.target.value,
                    );
                    field.onChange(formattedUrl);
                    e.target.value = formattedUrl;
                  }
                  : field.onBlur}
              />
            )}
          />
        </div>
      );

    case "textarea":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <textarea
                {...field}
                className={`w-full px-4 py-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
                  item.classList || ""
                }`}
                placeholder={item.placeholder || ""}
                rows={item.rows || 4}
              />
            )}
          />
        </div>
      );

    case "select":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <select
                {...field}
                className={`w-full px-4 py-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
                  item.classList || ""
                }`}
              >
                {item.options?.map((opt: { label: string; value: string }) => (
                  <option key={opt.value} value={opt.value}>
                    {opt.label}
                  </option>
                ))}
              </select>
            )}
          />
        </div>
      );

    case "dropdown":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field: { onChange, value } }) => (
              <div className="relative">
                <button
                  ref={buttonRef}
                  type="button"
                  className={`w-full px-4 py-2 text-left text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
                    item.classList || ""
                  }`}
                  onClick={() => setDropdownOpen(!dropdownOpen)}
                >
                  {item.options?.find((opt) => opt.value === value)?.label ||
                    value}
                </button>
                {dropdownOpen && portalContainer && createPortal(
                  <div
                    ref={dropdownRef}
                    style={dropdownStyle}
                    className="fixed bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md shadow-lg overflow-y-auto scrollbar-thin"
                  >
                    <ul>
                      {item.options?.map((opt) => (
                        <li
                          key={opt.value}
                          className="flex items-center px-4 py-2 hover:bg-gray-100 dark:hover:bg-gray-600 cursor-pointer"
                          onClick={() => {
                            onChange(opt.value);
                            setDropdownOpen(false);
                          }}
                        >
                          {opt.icon
                            ? (
                              <img
                                src={opt.icon}
                                className="w-4 h-4 mr-2"
                                alt=""
                              />
                            )
                            : null}
                          <span>{opt.label}</span>
                        </li>
                      ))}
                    </ul>
                  </div>,
                  portalContainer,
                )}
              </div>
            )}
          />
        </div>
      );

    case "checkbox":
      return (
        <div className="mb-4 w-full">
          <label className="flex items-center cursor-pointer">
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "false")}
              render={({ field: { onChange, value } }) => (
                <input
                  type="checkbox"
                  className={`mr-2 h-4 w-4 text-[#0061E0] bg-white dark:bg-[#42414D] border-gray-300 dark:border-[#42414D] rounded focus:ring-[#0061E0] ${
                    item.classList || ""
                  }`}
                  checked={value === "true"}
                  onChange={(e) => onChange(String(e.target.checked))}
                />
              )}
            />
            <span className="text-sm text-gray-900 dark:text-white">
              {item.label}
            </span>
          </label>
        </div>
      );

    case "radio":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <div className="space-y-2">
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "")}
              render={({ field: { onChange, value } }) => (
                <>
                  {item.options?.map(
                    (opt: { label: string; value: string }) => (
                      <label
                        key={opt.value}
                        className="flex items-center cursor-pointer mb-2"
                      >
                        <input
                          type="radio"
                          className={`mr-2 h-4 w-4 text-[#0061E0] bg-white dark:bg-[#42414D] border-gray-300 dark:border-[#42414D] focus:ring-[#0061E0] ${
                            item.classList || ""
                          }`}
                          value={opt.value}
                          checked={value === opt.value}
                          onChange={(e) => onChange(e.target.value)}
                        />
                        <span className="text-sm text-gray-900 dark:text-white">
                          {opt.label}
                        </span>
                      </label>
                    ),
                  )}
                </>
              )}
            />
          </div>
        </div>
      );

    default:
      return null;
  }
};

interface NoraModalInitMessage {
  type: "nora-modal-init";
  detail: string | TForm;
}

interface NoraModalRemoveMessage {
  type: "nora-modal-remove";
}

interface NoraModalSubmitMessage {
  type: "nora-modal-submit";
  detail: FormValues | null;
}

type NoraMessage =
  | NoraModalInitMessage
  | NoraModalRemoveMessage
  | NoraModalSubmitMessage;

function isNoraMessage(data: unknown): data is NoraMessage {
  return (
    typeof data === "object" &&
    data !== null &&
    "type" in data &&
    typeof (data as { type: unknown }).type === "string" &&
    (data as { type: string }).type.startsWith("nora-modal-")
  );
}

function postNoraMessage(message: NoraMessage) {
  // 1. Post to current window (for actor listening on window)
  window.postMessage(message, "*");

  // 2. Post to opener/parent if available
  if (window.opener || window.parent !== window) {
    const target = window.opener || window.parent;
    target.postMessage(message, "*");
  }
}

function App() {
  const methods = useForm<FormValues>();
  const { control, watch } = methods;
  const [formConfig, setFormConfig] = useState<TForm | null>(null);

  const values = watch();

  const getVisibleFormItems = (items: TFormItem[]): TFormItem[] => {
    if (!items) return [];

    return items.filter((item) => {
      if (!item.when) return true;

      const { id, value } = item.when;
      const currentValue = values[id];

      if (Array.isArray(value)) {
        return value.includes(currentValue);
      }

      return currentValue === value;
    });
  };

  const onSubmit = (data: FormValues) => {
    postNoraMessage({ type: "nora-modal-submit", detail: data });

    if (typeof globalThis.sendForm === "function") {
      globalThis.sendForm(data);
    }
  };

  const handleCancel = () => {
    postNoraMessage({ type: "nora-modal-submit", detail: null });

    if (typeof globalThis.sendForm === "function") {
      globalThis.sendForm(null);
    }
  };

  useEffect(() => {
    const handler = (e: Event) => {
      if (e instanceof CustomEvent) {
        methods.reset(e.detail);
      }
    };

    const messageHandler = (e: MessageEvent) => {
      if (!isNoraMessage(e.data)) {
        return;
      }

      const message = e.data;

      if (message.type === "nora-modal-init") {
        try {
          const detail = message.detail;
          const config = typeof detail === "string"
            ? JSON.parse(detail)
            : detail;

          if (!config || !config.forms) {
            console.error("Invalid config received:", config);
            return;
          }

          setFormConfig(config);
          const initialValues: FormValues = {};
          config.forms.forEach((item: TFormItem) => {
            initialValues[item.id] = String(item.value || "");
          });
          methods.reset(initialValues);
        } catch (err) {
          console.error("Failed to parse modal config:", err);
        }
      } else if (message.type === "nora-modal-remove") {
        setFormConfig(null);
      }
    };

    globalThis.addEventListener("form-update", handler);
    globalThis.addEventListener("message", messageHandler);

    // Signal readiness
    document.documentElement.dataset.noraModalReady = "true";

    return () => {
      globalThis.removeEventListener("form-update", handler);
      globalThis.removeEventListener("message", messageHandler);
    };
  }, [methods]);

  globalThis.buildFormFromConfig = (config: TForm) => {
    setFormConfig(config);
    const initialValues: FormValues = {};
    config.forms.forEach((item) => {
      initialValues[item.id] = String(item.value || "");
    });
    methods.reset(initialValues);
  };

  globalThis.removeForm = () => {
    setFormConfig(null);
  };

  return (
    <>
      <div className="w-full h-screen flex bg-white dark:bg-[#2B2A33] text-gray-900 dark:text-white">
        <FormProvider {...methods}>
          <form
            id="dynamic-form"
            className="w-full flex flex-col bg-white dark:bg-[#2B2A33] p-4"
            onSubmit={methods.handleSubmit(onSubmit)}
          >
            {formConfig?.title && (
              <div className="flex-none py-3 px-1 border-b border-gray-200 dark:border-gray-700">
                <h1 className="text-xl font-bold text-gray-900 dark:text-white">
                  {formConfig.title}
                </h1>
              </div>
            )}

            <div className="flex-grow overflow-y-auto py-4 px-1 flex flex-col gap-4">
              {formConfig &&
                getVisibleFormItems(formConfig.forms).map((item: TFormItem) => (
                  <FormField key={item.id} item={item} control={control} />
                ))}
            </div>

            {formConfig && (
              <div className="flex-none pt-3 pb-1 px-1 border-t border-gray-200 dark:border-gray-700 flex justify-end gap-3">
                <button
                  type="submit"
                  className="px-5 py-2 text-sm font-medium text-white bg-[#0061E0] hover:bg-[#0250BC] rounded-md shadow-sm transition duration-150 ease-in-out"
                >
                  {formConfig.submitLabel || "Submit"}
                </button>
                {formConfig.cancelLabel && (
                  <button
                    type="button"
                    onClick={handleCancel}
                    className="px-5 py-2 text-sm font-medium text-gray-900 dark:text-white bg-gray-200 dark:bg-[#42414D] hover:bg-gray-300 dark:hover:bg-[#53525C] rounded-md transition duration-150 ease-in-out"
                  >
                    {formConfig.cancelLabel}
                  </button>
                )}
              </div>
            )}
          </form>
        </FormProvider>
      </div>
    </>
  );
}

export default App;
