export interface HistoryItem {
  url: string;
  title: string;
  lastVisitDate: number;
  visitCount: number;
}

export interface Tab {
  id: number;
  url: string;
  title: string;
  isActive: boolean;
  isPinned: boolean;
}

export interface Download {
  id: number;
  url: string;
  filename: string;
  status: string;
  startTime: number;
}
