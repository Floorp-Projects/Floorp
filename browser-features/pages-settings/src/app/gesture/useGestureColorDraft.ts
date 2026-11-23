import { useEffect, useRef, useState } from "react";

/** Keep typing responsive while gesture preferences are saved in order. */
export function useGestureColorDraft(
  value: string,
  save: (value: string) => Promise<boolean>,
): [string, (value: string) => void] {
  const [draft, setDraft] = useState(value);
  const latestValue = useRef(value);
  const latestEdit = useRef(0);
  const pending = useRef(0);
  latestValue.current = value;

  useEffect(() => {
    if (pending.current === 0) setDraft(value);
  }, [value]);

  const update = (next: string) => {
    setDraft(next);
    const edit = ++latestEdit.current;
    pending.current++;
    void save(next).then((saved) => {
      pending.current--;
      if (!saved && edit === latestEdit.current) setDraft(latestValue.current);
    }, () => {
      pending.current--;
      if (edit === latestEdit.current) setDraft(latestValue.current);
    });
  };

  return [draft, update];
}
